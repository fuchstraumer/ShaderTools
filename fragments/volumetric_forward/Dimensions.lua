local dimensions = {
    LightGridBlockSize = 32;
    ClusterGridBlockSize = 64;
    AverageLightsPerTile = 100;
};

function dimensions.ClusterDimensions()
    cluster_x = math.ceil(GetWindowX() / dimensions.ClusterGridBlockSize);
    cluster_y = math.ceil(GetWindowY() / dimensions.ClusterGridBlockSize);
    s_d = 2.0 * math.tan(math.rad(GetFieldOfViewY()) * 0.50) / cluster_y;
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