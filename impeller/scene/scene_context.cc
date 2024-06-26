// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/scene/scene_context.h"
#include "impeller/core/formats.h"
#include "impeller/core/host_buffer.h"
#include "impeller/scene/material.h"
#include "impeller/scene/shaders/skinned.vert.h"
#include "impeller/scene/shaders/unlit.frag.h"
#include "impeller/scene/shaders/unskinned.vert.h"

namespace impeller {
namespace scene {

void SceneContextOptions::ApplyToPipelineDescriptor(
    const Capabilities& capabilities,
    PipelineDescriptor& desc) const {
  DepthAttachmentDescriptor depth;
  depth.depth_compare = CompareFunction::kLess;
  depth.depth_write_enabled = true;
  desc.SetDepthStencilAttachmentDescriptor(depth);
  desc.SetDepthPixelFormat(capabilities.GetDefaultDepthStencilFormat());

  StencilAttachmentDescriptor stencil;
  stencil.stencil_compare = CompareFunction::kAlways;
  stencil.depth_stencil_pass = StencilOperation::kKeep;
  desc.SetStencilAttachmentDescriptors(stencil);
  desc.SetStencilPixelFormat(capabilities.GetDefaultDepthStencilFormat());

  desc.SetSampleCount(sample_count);
  desc.SetPrimitiveType(primitive_type);

  desc.SetWindingOrder(WindingOrder::kCounterClockwise);
  desc.SetCullMode(CullMode::kBackFace);
}

SceneContext::SceneContext(std::shared_ptr<Context> context)
    : context_(std::move(context)) {
  if (!context_ || !context_->IsValid()) {
    return;
  }

  auto unskinned_variant =
      MakePipelineVariants<UnskinnedVertexShader, UnlitFragmentShader>(
          *context_);
  if (!unskinned_variant) {
    FML_LOG(ERROR) << "Could not create unskinned pipeline variant.";
    return;
  }
  pipelines_[{PipelineKey{GeometryType::kUnskinned, MaterialType::kUnlit}}] =
      std::move(unskinned_variant);

  auto skinned_variant =
      MakePipelineVariants<SkinnedVertexShader, UnlitFragmentShader>(*context_);
  if (!skinned_variant) {
    FML_LOG(ERROR) << "Could not create skinned pipeline variant.";
    return;
  }
  pipelines_[{PipelineKey{GeometryType::kSkinned, MaterialType::kUnlit}}] =
      std::move(skinned_variant);

  {
    impeller::TextureDescriptor texture_descriptor;
    texture_descriptor.storage_mode = impeller::StorageMode::kHostVisible;
    texture_descriptor.format = PixelFormat::kR8G8B8A8UNormInt;
    texture_descriptor.size = {1, 1};
    texture_descriptor.mip_count = 1u;

    placeholder_texture_ =
        context_->GetResourceAllocator()->CreateTexture(texture_descriptor);
    placeholder_texture_->SetLabel("Placeholder Texture");
    if (!placeholder_texture_) {
      FML_LOG(ERROR) << "Could not create placeholder texture.";
      return;
    }

    uint8_t pixel[] = {0xFF, 0xFF, 0xFF, 0xFF};
    if (!placeholder_texture_->SetContents(pixel, 4)) {
      FML_LOG(ERROR) << "Could not set contents of placeholder texture.";
      return;
    }
  }
  host_buffer_ = HostBuffer::Create(GetContext()->GetResourceAllocator());
  is_valid_ = true;
}

SceneContext::~SceneContext() = default;

std::shared_ptr<Pipeline<PipelineDescriptor>> SceneContext::GetPipeline(
    PipelineKey key,
    SceneContextOptions opts) const {
  if (!IsValid()) {
    return nullptr;
  }
  if (auto found = pipelines_.find(key); found != pipelines_.end()) {
    return found->second->GetPipeline(*context_, opts);
  }
  return nullptr;
}

bool SceneContext::IsValid() const {
  return is_valid_;
}

std::shared_ptr<Context> SceneContext::GetContext() const {
  return context_;
}

std::shared_ptr<Texture> SceneContext::GetPlaceholderTexture() const {
  return placeholder_texture_;
}

}  // namespace scene
}  // namespace impeller
