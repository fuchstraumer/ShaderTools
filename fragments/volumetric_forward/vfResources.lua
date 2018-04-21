LightGridBlockSize = 32;
ClusterGridBlockSize = 64;
AverageLightsPerTile = 100;

function get_cluster_dimensions()
    cluster_x = math.ceil(GetWindowX() / ClusterGridBlockSize);
    cluster_y = math.ceil(GetWindowY() / ClusterGridBlockSize);
    s_d = 2.0 * math.tan(math.rad(GetFieldOfViewY()) * 0.50) / cluster_y;
    log_dim_y = 1.0 / math.log(1.0 + s_d);
    log_depth = math.abs(math.log(GetZNear() / GetZFar()));
    cluster_z = math.floor(log_depth * log_dim_y);
    return cluster_x, cluster_y, cluster_z;
end

function get_num_clusters()
    x, y, z = get_cluster_dimensions();
    return x * y * z;
end

function get_num_threads()
   threads_x = math.ceil(GetWindowX() / LightGridBlockSize);
   threads_y = math.ceil(GetWindowY() / LightGridBlockSize);
   return threads_x, threads_y, 1; 
end

function get_num_thread_groups()
    tx, ty, tz = get_num_threads();
    groups_x = math.ceil(tx / LightGridBlockSize);
    groups_y = math.ceil(ty / LightGridBlockSize);
    return groups_x, groups_y, 1;
end

function light_index_list_size()
    tx, ty, tz = get_num_threads();
    return tx * ty * tz * AverageLightsPerTile;
end

function light_grid_size()
    tx, ty, tz = get_num_threads();
    return tx * ty * tz;
end

function get_num_point_lights()
    return 8192;
end

function get_num_spot_lights()
    return 8192;
end

function get_num_directional_lights()
    return 1024;
end

function get_num_light_aabbs()
    return 512;
end

Resources = {
    VolumetricForward = {
        ClusterData = {
            Type = "UniformBuffer",
            Members = {
                GridDim = "uvec3",
                ViewNear = "float",
                ScreenSize = "uvec2",
                Near = "float",
                LogGridDimY = "float"
            }
        },
        ClusterAABBs = {
            Type = "StorageBuffer",
            ElementType = "AABB",
            NumElements = get_num_clusters()
        },
        ClusterFlags = {
            Type = "StorageImage",
            Format = "r8ui",
            Size = get_num_clusters()
        },
        PointLightIndexList = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = light_index_list_size()
        },
        SpotLightIndexList = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = light_index_list_size()
        },
        PointLightGrid = {
            Type = "StorageImage",
            Format = "rg32ui",
            Size = light_grid_size()
        },
        SpotLightGrid = {
            Type = "StorageImage",
            Format = "rg32ui",
            Size = light_grid_size()
        },
        PointLightIndexCounter = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = 1
        },
        SpotLightIndexCounter = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = 1
        },
        UniqueClustersCounter = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = 1
        },
        UniqueClusters = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = get_num_clusters()
        }
    },
    VolumetricForwardLights = {
        LightCounts = {
            Type = "UniformBuffer",
            Members = {
                NumPointLights = "uint",
                NumSpotLights = "uint",
                NumDirectionalLights = "uint"
            }
        },
        PointLights = {
            Type = "StorageBuffer",
            ElementType = "PointLight",
            NumElements = get_num_point_lights()
        },
        SpotLights = {
            Type = "StorageBuffer",
            ElementType = "SpotLight",
            NumElements = get_num_spot_lights()
        },
        DirectionalLights = {
            Type = "StorageBuffer",
            ElementType = "DirectionalLight",
            NumElements = get_num_directional_lights()
        }
    },
    IndirectArgsSet = {
        IndirectArgs = {
            Type = "UniformBuffer",
            Members = {
                NumThreadGroupsX = "uint",
                NumThreadGroupsY = "uint",
                NumThreadGroupsZ = "uint"
            }
        }       
    },
    SortResources = {
        DispatchParams = {
            Type = "UniformBuffer",
            Members = {
                NumThreadGroups = "uvec3",
                NumThreads = "uvec3"
            }
        },
        ReductionParams = {
            Type = "UniformBuffer",
            Members = {
                NumElements = "uint"
            }
        },
        SortParams = {
            Type = "UniformBuffer",
            Members = {
                NumElements = "uint",
                ChunkSize = "uint"
            }
        },
        LightAABBs = {
            Type = "StorageBuffer",
            ElementType = "AABB",
            NumElements = get_num_light_aabbs()
        },
        PointLightMortonCodes = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = get_num_point_lights()
        },
        SpotLightMortonCodes = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = get_num_spot_lights()
        },
        PointLightIndices = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = get_num_point_lights()
        },
        SpotLightIndices = {
            Type = "StorageImage",
            Format = "r32ui",
            Size = get_num_spot_lights()
        }
    },
    BVHResources = {
        BVHParams = {
            Type = "UniformBuffer",
            Members = {
                PointLightLevels = "uint",
                SpotLightLevels = "uint",
                ChildLevel = "uint"
            }
        },
        PointLightBVH = {
            Type = "StorageBuffer",
            ElementType = "AABB",
            NumElements = get_num_point_lights()
        },
        SpotLightBVH = {
            Type = "StorageBuffer",
            ElementType = "AABB",
            NumElements = get_num_spot_lights()
        }
    }
}
