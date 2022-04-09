idf_component_register(
	SRCS
    "src/lv_pngle.c"
    "src/external/src/pngle.c"
    "src/external/src/miniz.c"
  
	INCLUDE_DIRS
		"src"
    
    REQUIRES main
)
