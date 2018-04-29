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

ObjectSizes = {
    PointLight = 64,
    SpotLight = 96,
    DirectionalLight = 64,
    AABB = 32,
    Plane = 16,
    Frustum = 64,
    Sphere = 16,
    Cone = 32,
    LightingResult = 48,
    Material = 128
}

Resources = {
    GlobalResources = {
        matrices = {
            Type = "UniformBuffer",
            Members = {
                model = { "mat4", 0 },
                view = { "mat4", 1 },
                projection = { "mat4", 2 },
                normal = { "mat4", 3 }
            }
        },
        globals = {
            Type = "UniformBuffer",
            Members = {
                viewPosition = { "vec4", 0 },
                mousePosition = { "vec2", 1 },
                windowSize = { "vec2", 2 },
                depthRange = { "vec2", 3 },
                frame = { "uint", 4 }
            }
        },
        lightingData = {
            Type = "UniformBuffer",
            Members = {
                Exposure = { "float", 0 },
                Gamma = { "float", 1 }
            }
        }
    },
    VolumetricForward = {
        ClusterData = {
            Type = "UniformBuffer",
            Members = {
                GridDim = { "uvec3", 0 },
                ViewNear = { "float", 1 },
                ScreenSize = { "uvec2", 2 },
                Near = { "float", 3 },
                LogGridDimY = { "float", 4 }
            }
        },
        ClusterAABBs = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_clusters()
                }, 0 }
            }
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
                NumPointLights = { "uint", 0 },
                NumSpotLights = { "uint", 1 },
                NumDirectionalLights = { "uint", 2 }
            }
        },
        PointLights = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "PointLight",
                    NumElements = get_num_point_lights()
                }, 0 }
            }
        },
        SpotLights = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "SpotLight",
                    NumElements = get_num_spot_lights()
                }, 0 }
            }
        },
        DirectionalLights = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "DirectionalLight",
                    NumElements = get_num_directional_lights()
                }, 0 }
            }
        }
    },
    IndirectArgsSet = {
        IndirectArgs = {
            Type = "StorageBuffer",
            Members = {
                NumThreadGroupsX = { "uint", 0 },
                NumThreadGroupsY = { "uint", 1 },
                NumThreadGroupsZ = { "uint", 2 }
            }
        }       
    },
    SortResources = {
        DispatchParams = {
            Type = "UniformBuffer",
            Members = {
                NumThreadGroups = { "uvec3", 0 },
                NumThreads = { "uvec3", 1 }
            }
        },
        ReductionParams = {
            Type = "UniformBuffer",
            Members = {
                NumElements = { "uint", 0 }
            }
        },
        SortParams = {
            Type = "UniformBuffer",
            Members = {
                NumElements = { "uint", 0 },
                ChunkSize = { "uint", 1 }
            }
        },
        LightAABBs = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_light_aabbs()
                }, 0 }
            }
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
                PointLightLevels = { "uint", 0 },
                SpotLightLevels = { "uint", 1 },
                ChildLevel = { "uint", 2 }
            }
        },
        PointLightBVH = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_point_lights()
                }, 0 }
            }
        },
        SpotLightBVH = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_spot_lights()
                }, 0 }
            }
        }
    },
    MaterialResources = {
        diffuseMap = {
            Type = "CombinedImageSampler",
            ImageOptions = {
                SizeClass = "Absolute",
                SampleCount = 1,
                ImageType = "2D",
                GenerateMipMaps = false,
                MipLevels = 1,
                Layers = 1
            },
            SamplerOptions = {
                MagFilter = "Linear",
                MinFilter = "Linear",
                MipMapFilter = "Linear",
                AddressModeU = "Repeat",
                AddressModeV = "Repeat",
                AddressModeW = "ClampToEdge",
                EnableAnisotropy = true,
                MaxAnisotropy = 4.0
            }
        }
    }
}
