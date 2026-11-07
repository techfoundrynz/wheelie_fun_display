idf_component_register(SRCS "protocol.cpp" "telemetry.cpp" "integration.cpp" "wfb_config.cpp" "tuning.cpp" INCLUDE_DIRS "include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)

