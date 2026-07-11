#include "main.c"
#ifndef VULKAN_DEMO_DESCRIPTOR_C
#define VULKAN_DEMO_DESCRIPTOR_C

// descriptor.c - the common "one uniform buffer plus some sampled images"
// descriptor shape: binding 0 is a UBO visible to vertex + fragment stages,
// bindings 1..nr_samplers are combined image samplers for the fragment stage

#define VULKAN_MAX_BINDINGS 16 // 1 UBO + up to 15 samplers

void vulkan_create_descriptor_set_layout(int nr_samplers, VkDescriptorSetLayout *layout)
{
    assert(nr_samplers < VULKAN_MAX_BINDINGS);
    VkDescriptorSetLayoutBinding bindings[VULKAN_MAX_BINDINGS] = {0};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    for (int i = 0; i < nr_samplers; i++) {
        bindings[1 + i].binding = 1 + i;
        bindings[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1 + i].descriptorCount = 1;
        bindings[1 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1 + nr_samplers,
        .pBindings = bindings,
    };

    vkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, layout);
}

// make a pool and fill nr_sets descriptor sets from it, one per in-flight
// frame: set i points at ubos[i], and every set samples the same images
void vulkan_create_descriptor_sets(VkDescriptorSetLayout layout, int nr_sets,
    VkBuffer *ubos, size_t ubo_size,
    VkDescriptorImageInfo *samplers, int nr_samplers,
    VkDescriptorPool *pool, VkDescriptorSet *sets)
{
    assert(nr_samplers < VULKAN_MAX_BINDINGS);

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nr_sets },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nr_samplers * nr_sets },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = nr_sets,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes,
    };
    vkCreateDescriptorPool(vk.device, &pool_info, NULL, pool);

    VkDescriptorSetLayout *layouts = malloc(nr_sets * sizeof *layouts);
    for (int i = 0; i < nr_sets; i++)
        layouts[i] = layout;

    VkDescriptorSetAllocateInfo set_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = *pool,
        .descriptorSetCount = nr_sets,
        .pSetLayouts = layouts,
    };
    vkAllocateDescriptorSets(vk.device, &set_alloc, sets);
    free(layouts);

    for (int i = 0; i < nr_sets; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = ubos[i],
            .offset = 0,
            .range = ubo_size,
        };

        VkWriteDescriptorSet writes[VULKAN_MAX_BINDINGS] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = sets[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
            },
        };

        for (int j = 0; j < nr_samplers; j++) {
            writes[1 + j] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = sets[i],
                .dstBinding = 1 + j,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &samplers[j],
            };
        }

        vkUpdateDescriptorSets(vk.device, 1 + nr_samplers, writes, 0, NULL);
    }
}

#endif // VULKAN_DEMO_DESCRIPTOR_C
