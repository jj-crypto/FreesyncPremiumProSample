// AMD AMDUtils code
//
// Copyright(c) 2019 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include "base\ShaderCompilerHelper.h"
#include "FlipBookAnimation.h"

namespace CAULDRON_VK
{
    void FlipBookAnimation::OnCreate(
        Device* pDevice,
        VkRenderPass renderPass,
        UploadHeap* pUploadHeap,
        ResourceViewHeaps *pResourceViewHeaps,
        DynamicBufferRing *pDynamicBufferRing,
        StaticBufferPool *pStaticBufferPool,
        VkSampleCountFlagBits sampleDescCount,
        UINT numRows, UINT numColms, std::string flipBookAnimationTexture, XMMATRIX worldMatrix)
    {
        m_pDevice = pDevice;
        m_pResourceViewHeaps = pResourceViewHeaps;
        m_pDynamicBufferRing = pDynamicBufferRing;
        m_pStaticBufferPool = pStaticBufferPool;
        m_sampleDescCount = sampleDescCount;
        m_numRows = numRows;
        m_numColms = numColms;
        m_worldMatrix = worldMatrix;
        m_NumIndices = 6;

        short indices[] =
        {
            0, 1, 2,
            2, 3, 0
        };
        m_indexType = VK_INDEX_TYPE_UINT16;
        m_pStaticBufferPool->AllocBuffer(m_NumIndices, sizeof(short), indices, &m_IBV);

        float vertices[] =
        {
            // pos                      // uv
            -1.0f,  1.0f, 0.0f, 1.0f,	0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f,	1.0f, 0.0f,
             1.0f, -1.0f, 0.0f, 1.0f,	1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,	0.0f, 1.0f
        };
        m_pStaticBufferPool->AllocBuffer(24, 6 * sizeof(float), vertices, &m_VBV);

        VkResult res;

        /////////////////////////////////////////////
        // vertex input state

        VkVertexInputBindingDescription vi_binding = {};
        vi_binding.binding = 0;
        vi_binding.stride = sizeof(float) * 6;
        vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vi_attrs[] =
        {
            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
            { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 4 },
        };

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext = NULL;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &vi_binding;
        vi.vertexAttributeDescriptionCount = _countof(vi_attrs);
        vi.pVertexAttributeDescriptions = vi_attrs;

        m_FlipBookTexture.InitFromFile(pDevice, pUploadHeap, flipBookAnimationTexture.c_str(), true);
        pUploadHeap->FlushAndFinish();
        m_FlipBookTexture.CreateSRV(&m_FlipBookTextureSRV);

        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        res = vkCreateSampler(pDevice->GetDevice(), &info, NULL, &m_sampler);
        assert(res == VK_SUCCESS);

        char vertexShaderFiles[1024] = "FlipBookAnimationVS.glsl";
        char pixelShaderFiles[1024] = "FlipBookAnimationPS.glsl";

        // Compile shaders
        //
        DefineList attributeDefines;

        VkPipelineShaderStageCreateInfo m_vertexShader;
        res = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_VERTEX_BIT, vertexShaderFiles, "main", &attributeDefines, &m_vertexShader);
        assert(res == VK_SUCCESS);

        VkPipelineShaderStageCreateInfo m_fragmentShader;
        res = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_FRAGMENT_BIT, pixelShaderFiles, "main", &attributeDefines, &m_fragmentShader);
        assert(res == VK_SUCCESS);

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { m_vertexShader, m_fragmentShader };

        /////////////////////////////////////////////
        // Create descriptor set layout

        /////////////////////////////////////////////
        // Create descriptor set

        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(2);
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        layoutBindings[0].pImmutableSamplers = NULL;

        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBindings[1].pImmutableSamplers = NULL;

        m_pResourceViewHeaps->CreateDescriptorSetLayoutAndAllocDescriptorSet(&layoutBindings, &m_descriptorSetLayout, &m_descriptorSet);
        m_pDynamicBufferRing->SetDescriptorSet(0, sizeof(FlipBookAnimationCBuffer), m_descriptorSet);

        /////////////////////////////////////////////
        // Create the pipeline layout using the descriptoset

        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
        pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pPipelineLayoutCreateInfo.pNext = NULL;
        pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        pPipelineLayoutCreateInfo.pPushConstantRanges = NULL;
        pPipelineLayoutCreateInfo.setLayoutCount = (uint32_t)1;
        pPipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;

        res = vkCreatePipelineLayout(pDevice->GetDevice(), &pPipelineLayoutCreateInfo, NULL, &m_pipelineLayout);
        assert(res == VK_SUCCESS);

        /////////////////////////////////////////////
        // Create pipeline

        // input assembly state

        VkPipelineInputAssemblyStateCreateInfo ia;
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext = NULL;
        ia.flags = 0;
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // rasterizer state

        VkPipelineRasterizationStateCreateInfo rs;
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.pNext = NULL;
        rs.flags = 0;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.depthClampEnable = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.depthBiasEnable = VK_FALSE;
        rs.depthBiasConstantFactor = 0;
        rs.depthBiasClamp = 0;
        rs.depthBiasSlopeFactor = 0;
        rs.lineWidth = 1.0f;

        VkPipelineColorBlendAttachmentState att_state[1];
        att_state[0].colorWriteMask = 0xf;
        att_state[0].blendEnable = VK_TRUE;
        att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
        att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
        att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

        // Color blend state

        VkPipelineColorBlendStateCreateInfo cb;
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.flags = 0;
        cb.pNext = NULL;
        cb.attachmentCount = 1;
        cb.pAttachments = att_state;
        cb.logicOpEnable = VK_FALSE;
        cb.logicOp = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;

        std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS
        };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = NULL;
        dynamicState.pDynamicStates = dynamicStateEnables.data();
        dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

        // view port state

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.pNext = NULL;
        vp.flags = 0;
        vp.viewportCount = 1;
        vp.scissorCount = 1;
        vp.pScissors = NULL;
        vp.pViewports = NULL;

        // depth stencil state

        VkPipelineDepthStencilStateCreateInfo ds;
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.pNext = NULL;
        ds.flags = 0;
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;
        ds.back.failOp = VK_STENCIL_OP_KEEP;
        ds.back.passOp = VK_STENCIL_OP_KEEP;
        ds.back.compareOp = VK_COMPARE_OP_LESS;
        ds.back.compareMask = 0;
        ds.back.reference = 0;
        ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
        ds.back.writeMask = 0;
        ds.minDepthBounds = 0;
        ds.maxDepthBounds = 0;
        ds.stencilTestEnable = VK_FALSE;
        ds.front = ds.back;

        // multi sample state

        VkPipelineMultisampleStateCreateInfo ms;
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext = NULL;
        ms.flags = 0;
        ms.pSampleMask = NULL;
        ms.rasterizationSamples = sampleDescCount;
        ms.sampleShadingEnable = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable = VK_FALSE;
        ms.minSampleShading = 0.0;

        // create pipeline

        VkGraphicsPipelineCreateInfo pipeline = {};
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.pNext = NULL;
        pipeline.layout = m_pipelineLayout;
        pipeline.basePipelineHandle = VK_NULL_HANDLE;
        pipeline.basePipelineIndex = 0;
        pipeline.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
        pipeline.pVertexInputState = &vi;
        pipeline.pInputAssemblyState = &ia;
        pipeline.pRasterizationState = &rs;
        pipeline.pColorBlendState = &cb;
        pipeline.pTessellationState = NULL;
        pipeline.pMultisampleState = &ms;
        pipeline.pDynamicState = &dynamicState;
        pipeline.pViewportState = &vp;
        pipeline.pDepthStencilState = &ds;
        pipeline.pStages = shaderStages.data();
        pipeline.stageCount = (uint32_t)shaderStages.size();
        pipeline.renderPass = renderPass;
        pipeline.subpass = 0;

        res = vkCreateGraphicsPipelines(pDevice->GetDevice(), pDevice->GetPipelineCache(), 1, &pipeline, NULL, &m_pipeline);
        assert(res == VK_SUCCESS);

        SetDescriptorSet(m_pDevice->GetDevice(), 1, m_FlipBookTextureSRV, &m_sampler, m_descriptorSet);
    }

    void FlipBookAnimation::OnDestroy()
    {
        vkDestroyImageView(m_pDevice->GetDevice(), m_FlipBookTextureSRV, NULL);
        m_FlipBookTexture.OnDestroy();
        m_pResourceViewHeaps->FreeDescriptor(m_descriptorSet);
        vkDestroyPipeline(m_pDevice->GetDevice(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_pDevice->GetDevice(), m_pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_descriptorSetLayout, NULL);
        vkDestroySampler(m_pDevice->GetDevice(), m_sampler, nullptr);
    }

    void FlipBookAnimation::Draw(VkCommandBuffer cmd_buf, float time, XMVECTOR camPos, XMMATRIX viewProjMat)
    {
        VkDescriptorBufferInfo cbFlipBookAnimation;
        FlipBookAnimationCBuffer *pFlipBookAnimation;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(FlipBookAnimationCBuffer), (void **)&pFlipBookAnimation, &cbFlipBookAnimation);
        pFlipBookAnimation->row = m_numRows;
        pFlipBookAnimation->cols = m_numColms;
        pFlipBookAnimation->time = time;

        XMFLOAT4 dist;
        XMStoreFloat4(&dist, camPos - m_worldMatrix.r[3]);
        float angle = atan2(dist.x, dist.z);

        pFlipBookAnimation->angle = -angle;
        pFlipBookAnimation->camPos = camPos;
        pFlipBookAnimation->worldMat = m_worldMatrix;
        pFlipBookAnimation->viewProjMat = viewProjMat;

        VkDescriptorSet descritorSets[1] = { m_descriptorSet };

        uint32_t uniformOffsets[1] = { (uint32_t)cbFlipBookAnimation.offset };
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, descritorSets, 1, uniformOffsets);

        vkCmdBindIndexBuffer(cmd_buf, m_IBV.buffer, m_IBV.offset, m_indexType);
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &m_VBV.buffer, &m_VBV.offset);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        vkCmdDrawIndexed(cmd_buf, m_NumIndices, 1, 0, 0, 0);
	}
}
