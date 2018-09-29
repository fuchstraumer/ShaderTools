local dimensions = {
    LightGridBlockSize = 32;
    ClusterGridBlockSize = 64;
    AverageLightsPerTile = 100;
    SortNumThreadsPerThreadGroup = 256;
    SortElementsPerThread = 8;
    MaxSpotLights = 4096;
    MaxPointLights = 4096;
    NearK = 0.0;
};

function dimensions.get_sD()
    cluster_x = math.ceil(GetWindowX() / dimensions.ClusterGridBlockSize);
    cluster_y = math.ceil(GetWindowY() / dimensions.ClusterGridBlockSize);
    return 2.0 * math.tan(math.rad(GetFieldOfViewY()) * 0.50) / cluster_y;
end

function dimensions.SetNearK()
    s_d = dimensions.get_sD();
    dimensions.NearK = 1.0 + s_d;
end

function dimensions.ClusterDimensions()
    s_d = dimensions.get_sD();
    log_dim_y = 1.0 / math.log(1.0 + s_d);
    log_depth = math.abs(math.log(GetZNear() / GetZFar()));
    cluster_z = math.floor(log_depth * log_dim_y);
    return cluster_x, cluster_y, cluster_z;
end

function dimensions.NumClusters()
    x, y, z = dimensions.ClusterDimensions();
    return x * y * z;
end

function dimensions.NumThreads()
    threads_x = math.ceil(GetWindowX() / dimensions.LightGridBlockSize);
    threads_y = math.ceil(GetWindowY() / dimensions.LightGridBlockSize);
    return threads_x, threads_y, 1; 
end

function dimensions.NumThreadGroups()
    tx, ty, tz = dimensions.NumThreads();
    groups_x = math.ceil(tx / dimensions.LightGridBlockSize);
    groups_y = math.ceil(ty / dimensions.LightGridBlockSize);
    return groups_x, groups_y, 1;
end

function dimensions.LightIndexListSize()
    tx, ty, tz = dimensions.NumThreads();
    return tx * ty * tz * dimensions.AverageLightsPerTile;
end

function dimensions.MaxMergePathPartitions()
    num_chunks = math.ceil(dimensions.MaxPointLights / dimensions.SortNumThreadsPerThreadGroup);
    max_sort_groups = num_chunks / 2;
    merge_path_partitions = math.ceil(dimensions.SortNumThreadsPerThreadGroup * 2) / 
        (dimensions.SortElementsPerThread * dimensions.SortNumThreadsPerThreadGroup) + 1;
    return merge_path_partitions * max_sort_groups;
end

function dimensions.LightGridSize()
    tx, ty, tz = dimensions.NumThreads();
    return tx * ty * tz;
end

function dimensions.NumPointLights()
    return 8192;
end

function dimensions.NumSpotLights()
    return 8192;
end

function dimensions.NumDirectionalLights()
    return 1024;
end

function dimensions.NumLightAABBs()
    return 512;
end

return dimensions;