# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\Visual_Thermal_Concept_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\Visual_Thermal_Concept_autogen.dir\\ParseCache.txt"
  "Visual_Thermal_Concept_autogen"
  )
endif()
