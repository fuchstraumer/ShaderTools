#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(constant_id = 0) const uint SortAlgorithm = 0u;

layout(set = 0, binding = 7, std140) uniform sort_params
{
    uint NumElements;
    uint ChunkSize;
} SortParams;

layout(set = 1, binding = 1, r32ui) uniform readonly uimageBuffer InputKeys;
layout(set = 1, binding = 3, r32ui) uniform writeonly uimageBuffer OutputKeys;
layout(set = 1, binding = 4, r32ui) uniform writeonly uimageBuffer OutputValues;
layout(set = 1, binding = 0, r32i) uniform iimageBuffer MergePathPartitions;
layout(set = 1, binding = 2, r32ui) uniform readonly uimageBuffer InputValues;

shared uint gs_Keys[2048];
shared uint gs_Values[2048];

uint _1243;

void main()
{
    switch (SortAlgorithm)
    {
        case 0:
        {
            uint _649 = min(SortParams.ChunkSize * 2u, SortParams.NumElements);
            uint _665 = uint(ceil(float(_649) * 0.00048828125) + 1.0);
            uint _669 = gl_GlobalInvocationID.x / _665;
            uint _673 = gl_GlobalInvocationID.x % _665;
            uint _678 = (_669 * _665) + _673;
            if (_678 < ((uint(ceil(float(SortParams.NumElements) / float(SortParams.ChunkSize))) / 2u) * _665))
            {
                int _690 = int(_669 * _649);
                int _698 = int(min(uint(_690) + SortParams.ChunkSize, SortParams.NumElements));
                int _701 = _698 - _690;
                int _713 = int(min(uint(_698) + SortParams.ChunkSize, SortParams.NumElements)) - _698;
                int _722 = int(min(_673 * 2048u, uint(_701 + _713)));
                int _1244;
                _1244 = max(0, _722 - _713);
                int _760;
                bool _801;
                int _1260;
                for (int _1245 = min(_722, _701); _1244 < _1245; _1245 = _801 ? _1245 : _760, _1244 = _1260)
                {
                    _760 = (_1244 + _1245) >> 1;
                    _801 = imageLoad(InputKeys, _690 + _760).x < imageLoad(InputKeys, ((_698 + _722) - 1) - _760).x;
                    if (_801)
                    {
                        _1260 = _760 + 1;
                        continue;
                    }
                    else
                    {
                        _1260 = _1244;
                        continue;
                    }
                    continue;
                }
                imageStore(MergePathPartitions, int(_678), ivec4(_1244, 0, 0, 0));
            }
            break;
        }
        case 1:
        {
            uint _868 = min(SortParams.ChunkSize * 2u, SortParams.NumElements);
            uint _873 = uint(ceil(float(_868) * 0.00048828125));
            uint _879 = gl_GlobalInvocationID.x / _873;
            uint _883 = gl_GlobalInvocationID.x % _873;
            uint _888 = (_879 * (_873 + 1u)) + _883;
            ivec4 _892 = imageLoad(MergePathPartitions, int(_888));
            int _893 = _892.x;
            ivec4 _898 = imageLoad(MergePathPartitions, int(_888 + 1u));
            int _899 = _898.x;
            int _904 = int(min(_883 * 2048u, _868));
            int _917 = int(min(_879 * _868, SortParams.NumElements));
            int _926 = int(min(uint(_917) + SortParams.ChunkSize, SortParams.NumElements));
            int _929 = _926 - _917;
            int _941 = int(min(uint(_926) + SortParams.ChunkSize, SortParams.NumElements)) - _926;
            int _950 = min(_899 - _893, _929);
            int _953 = _904 - _893;
            int _961 = min((int(min((_883 + 1u) * 2048u, _868)) - _899) - _953, _941);
            int _964 = int(gl_LocalInvocationIndex) * 8;
            int _1015;
            uint _1240;
            uint _1241;
            uint _1242;
            for (int _1212 = 0; uint(_1212) < 8u; _1015 = _964 + _1212, gs_Keys[_1015] = _1240, gs_Values[_1015] = _1241, _1242 = _1241, _1212++)
            {
                int _977 = (_893 + _964) + _1212;
                if (_977 < _899)
                {
                    int _992 = _917 + _977;
                    _1241 = imageLoad(InputValues, _992).x;
                    _1240 = imageLoad(InputKeys, _992).x;
                    continue;
                }
                else
                {
                    _1241 = _1242;
                    _1240 = imageLoad(InputKeys, _926 + (_953 + (_977 - _899))).x;
                    continue;
                }
                continue;
            }
            groupMemoryBarrier();
            int _1213;
            _1213 = max(0, _964 - _961);
            int _1073;
            bool _1114;
            int _1248;
            for (int _1214 = min(_964, _950); _1213 < _1214; _1214 = _1114 ? _1214 : _1073, _1213 = _1248)
            {
                _1073 = (_1213 + _1214) >> 1;
                _1114 = (gs_Keys[0 + _1073]) < (gs_Keys[((_950 + _964) - 1) - _1073]);
                if (_1114)
                {
                    _1248 = _1073 + 1;
                    continue;
                }
                else
                {
                    _1248 = _1213;
                    continue;
                }
                continue;
            }
            int _1034 = (_950 + _964) - _1213;
            int _1040 = _904 + _964;
            int _1215;
            int _1218;
            int _1220;
            uint _1222;
            uint _1224;
            uint _1228;
            uint _1233;
            _1233 = gs_Values[_1213];
            _1228 = gs_Values[_1034];
            _1224 = gs_Keys[_1034];
            _1222 = gs_Keys[_1213];
            _1220 = _1213;
            _1218 = _1034;
            _1215 = 0;
            int _1254;
            int _1255;
            uint _1256;
            uint _1257;
            uint _1258;
            uint _1259;
            for (;;)
            {
                uint _1145 = uint(_1215);
                bool _1146 = _1145 < 8u;
                bool _1154;
                if (_1146)
                {
                    _1154 = (_1040 + _1215) < int(uint(_929 + _941));
                }
                else
                {
                    _1154 = _1146;
                }
                if (_1154)
                {
                    int _1160 = (_917 + _1040) + _1215;
                    bool _1163 = _1218 >= (_950 + _961);
                    bool _1174;
                    if (!_1163)
                    {
                        _1174 = (_1220 < _950) && (_1222 < _1224);
                    }
                    else
                    {
                        _1174 = _1163;
                    }
                    if (_1174)
                    {
                        imageStore(OutputKeys, _1160, uvec4(_1222, 0u, 0u, 0u));
                        imageStore(OutputValues, _1160, uvec4(_1233, 0u, 0u, 0u));
                        int _1187 = _1220 + 1;
                        _1259 = gs_Values[_1187];
                        _1258 = _1228;
                        _1257 = _1224;
                        _1256 = gs_Keys[_1187];
                        _1255 = _1187;
                        _1254 = _1218;
                        _1233 = _1259;
                        _1228 = _1258;
                        _1224 = _1257;
                        _1222 = _1256;
                        _1220 = _1255;
                        _1218 = _1254;
                        _1215++;
                        continue;
                    }
                    else
                    {
                        imageStore(OutputKeys, _1160, uvec4(_1224, 0u, 0u, 0u));
                        imageStore(OutputValues, _1160, uvec4(_1228, 0u, 0u, 0u));
                        int _1203 = _1218 + 1;
                        _1259 = _1233;
                        _1258 = gs_Values[_1203];
                        _1257 = gs_Keys[_1203];
                        _1256 = _1222;
                        _1255 = _1220;
                        _1254 = _1203;
                        _1233 = _1259;
                        _1228 = _1258;
                        _1224 = _1257;
                        _1222 = _1256;
                        _1220 = _1255;
                        _1218 = _1254;
                        _1215++;
                        continue;
                    }
                    _1233 = _1259;
                    _1228 = _1258;
                    _1224 = _1257;
                    _1222 = _1256;
                    _1220 = _1255;
                    _1218 = _1254;
                    _1215++;
                    continue;
                }
                else
                {
                    break;
                }
            }
            break;
        }
    }
}

