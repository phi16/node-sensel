{
  "targets": [
    {
      "target_name": "sensel",
      "sources": [
        "sensel-lib.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<(module_root_dir)/include" 
      ],
      "libraries": [
        "<(module_root_dir)/lib/LibSensel.lib",
        "<(module_root_dir)/lib/LibSenselDecompress.lib"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ]
    }
  ]
}
