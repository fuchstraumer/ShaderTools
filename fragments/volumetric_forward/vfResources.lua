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
                model = "mat4",
                view = "mat4",
                projection = "mat4",
                normal = "mat4"
            }
        },
        globals = {
            Type = "UniformBuffer",
            Members = {
                viewPosition = "vec4",
                mousePosition = "vec2",
                windowSize = "vec2",
                depthRange = "vec2",
                frame = "uint"
            }
        },
        lightingData = {
            Type = "UniformBuffer",
            Members = {
                Exposure = "float",
                Gamma = "float"
            }
        }
    },
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
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_clusters()
                }
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
                NumPointLights = "uint",
                NumSpotLights = "uint",
                NumDirectionalLights = "uint"
            }
        },
        PointLights = {
            Type = "StorageBuffer",
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "PointLight",
                    NumElements = get_num_point_lights()
                }
            }
        },
        SpotLights = {
            Type = "StorageBuffer",
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "SpotLight",
                    NumElements = get_num_spot_lights()
                }
            }
        },
        DirectionalLights = {
            Type = "StorageBuffer",
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "DirectionalLight",
                    NumElements = get_num_directional_lights()
                }
            }
        }
    },
    IndirectArgsSet = {
        IndirectArgs = {
            Type = "StorageBuffer",
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
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_light_aabbs()
                }
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
                PointLightLevels = "uint",
                SpotLightLevels = "uint",
                ChildLevel = "uint"
            }
        },
        PointLightBVH = {
            Type = "StorageBuffer",
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_point_lights()
                }
            }
        },
        SpotLightBVH = {
            Type = "StorageBuffer",
            Members = {
                Data = {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = get_num_spot_lights()
                }
            }
        }
    }
}
