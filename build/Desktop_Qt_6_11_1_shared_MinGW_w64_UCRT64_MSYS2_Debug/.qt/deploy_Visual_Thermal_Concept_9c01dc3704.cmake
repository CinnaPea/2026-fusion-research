include("F:/qtRoadmap/Visual_Thermal_Concept/build/Desktop_Qt_6_11_1_shared_MinGW_w64_UCRT64_MSYS2_Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Visual_Thermal_Concept-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "F:/qtRoadmap/Visual_Thermal_Concept/build/Desktop_Qt_6_11_1_shared_MinGW_w64_UCRT64_MSYS2_Debug/Visual_Thermal_Concept.exe"
    GENERATE_QT_CONF
)
