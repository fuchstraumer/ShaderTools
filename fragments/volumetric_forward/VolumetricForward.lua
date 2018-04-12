WindowX = 1920;
WindowY = 1440;
Z_Near = 0.01;
Z_Far = 10000.0;
-- Degrees
FieldOfViewY = 75.0;

LightGridBlockSize = 32;
ClusterGridBlockSize = 64;
AverageLightsPerTile = 100;

function get_cluster_dimensions()
    cluster_x = math.ceil(WindowX / ClusterGridBlockSize);
    cluster_y = math.ceil(WindowY / ClusterGridBlockSize);
    s_d = 2.0 * math.tan(math.rad(FieldOfViewY) * 0.50) / cluster_y;
    log_dim_y = 1.0 / math.log(1.0 + s_d);
    log_depth = math.log(Z_Near / Z_Far);
    cluster_z = math.floor(log_depth * log_dim_y);
    return cluster_x, cluster_y, cluster_z;
end

function get_num_clusters()
    x, y, z = get_cluster_dimensions();
    return x * y * z;
end

function get_num_threads()
   threads_x = math.ceil(WindowX / LightGridBlockSize);
   threads_y = math.ceil(WindowY / LightGridBlockSize);
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
