cmake_minimum_required(VERSION 3.19)

project(Visual_Thermal_Concept LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc)
find_package(nlohmann_json CONFIG REQUIRED)

qt_standard_project_setup()

option(
        QART_ENABLE_REAL_DATA_TESTS
        "Enable parity tests requiring the external QART dataset root"
        OFF
)

set(
        QART_REAL_DATA_ROOT
        ""
        CACHE PATH
        "Absolute shared QART dataset root used by real-data parity tests"
)

set(
        QART_PHASE2_INPUT_FIXTURE_DIR
        ""
        CACHE PATH
        "Python thermal input/BYPASS parity fixture directory"
)

set(
        QART_PHASE2_N5_FIXTURE_DIR
        ""
        CACHE PATH
        "Python N5 numerical parity fixture directory"
)

set(
        QART_PHASE2_N5_REAL_FIXTURE_DIR
        ""
        CACHE PATH
        "Native-image Python N5 parity fixture directory"
)

option(
        QART_BUILD_RUNTIME_PROFILE
        "Build the Phase 2 thermal processing profiler"
        OFF
)

if(QART_BUILD_RUNTIME_PROFILE)
    add_executable(profile_phase2_thermal
            tools/profile_phase2_thermal.cpp
            src/application/application_controller.cpp
    )

    target_link_libraries(profile_phase2_thermal
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )
    add_executable(profile_phase2_n5_stages
            tools/profile_phase2_n5_stages.cpp
    )

    target_link_libraries(profile_phase2_n5_stages
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )
endif()

#option(
#        QART_BUILD_LEGACY_COMPARISON
#        "Build the saved legacy-output versus accepted N5 comparison"
#        OFF
#)
#
#if(QART_BUILD_LEGACY_COMPARISON)
#    add_executable(compare_n5_legacy
#            tools/compare_n5_legacy.cpp
#    )
#
#    target_link_libraries(compare_n5_legacy
#            PRIVATE
#            qart_core
#            ${OpenCV_LIBS}
#    )
#endif()

# ---------------------------------------------------------------------------
# QART-Fusion Qt-independent core
# ---------------------------------------------------------------------------

add_library(qart_core STATIC
        src/core/domain/datasets/dataset_partition_id.cpp
        src/core/domain/datasets/dataset_partition_id.h

        src/core/domain/datasets/dataset_usage_role.cpp
        src/core/domain/datasets/dataset_usage_role.h

        src/core/domain/datasets/experiment_view_assignment.cpp
        src/core/domain/datasets/experiment_view_assignment.h

        src/core/domain/datasets/experiment_view.cpp
        src/core/domain/datasets/experiment_view.h

        src/core/domain/imaging/image_dimensions.cpp
        src/core/domain/imaging/image_dimensions.h

        src/core/domain/datasets/dataset_pair.cpp
        src/core/domain/datasets/dataset_pair.h

        src/core/domain/validation/pair_validation_status.cpp
        src/core/domain/validation/pair_validation_status.h

        src/core/domain/validation/pair_validation_result.cpp
        src/core/domain/validation/pair_validation_result.h

        src/core/domain/imaging/image_decode_status.cpp
        src/core/domain/imaging/image_decode_status.h

        src/core/domain/imaging/image_decode_result.cpp
        src/core/domain/imaging/image_decode_result.h

        src/core/imaging/image_decoder.h

        src/core/imaging/opencv_image_decoder.cpp
        src/core/imaging/opencv_image_decoder.h

        src/core/domain/validation/pair_validation_policy.cpp
        src/core/domain/validation/pair_validation_policy.h

        src/core/domain/datasets/dataset_exceptions.h

        src/core/validation/pair_validator.cpp
        src/core/validation/pair_validator.h

        src/core/domain/validation/validation_status_count.cpp
        src/core/domain/validation/validation_status_count.h

        src/core/domain/validation/batch_validation_summary.cpp
        src/core/domain/validation/batch_validation_summary.h

        src/core/domain/validation/batch_validation_report.cpp
        src/core/domain/validation/batch_validation_report.h

        src/core/domain/validation/batch_validation_progress.cpp
        src/core/domain/validation/batch_validation_progress.h

        src/core/validation/batch_pair_validator.cpp
        src/core/validation/batch_pair_validator.h

        src/core/configuration/dataset_configuration.cpp
        src/core/configuration/dataset_configuration.h

        src/core/configuration/json_dataset_configuration_loader.cpp
        src/core/configuration/json_dataset_configuration_loader.h

        src/core/configuration/json_experiment_view_loader.cpp
        src/core/configuration/json_experiment_view_loader.h

        src/core/manifests/validation_manifest_row.cpp
        src/core/manifests/validation_manifest_row.h

        src/core/manifests/validation_manifest.cpp
        src/core/manifests/validation_manifest.h

        src/core/manifests/csv_validation_manifest_reader.cpp
        src/core/manifests/csv_validation_manifest_reader.h

        src/core/manifests/csv_validation_manifest_writer.cpp
        src/core/manifests/csv_validation_manifest_writer.h

        src/core/datasets/dataset_adapter.cpp
        src/core/datasets/dataset_adapter.h

        src/core/datasets/canonical_pair_lookup.cpp
        src/core/datasets/canonical_pair_lookup.h

        src/core/datasets/llvip_adapter.cpp
        src/core/datasets/llvip_adapter.h

        src/core/datasets/msrs_adapter.cpp
        src/core/datasets/msrs_adapter.h

        src/core/datasets/roadscene_adapter.cpp
        src/core/datasets/roadscene_adapter.h

        src/core/datasets/m3fd_adapter.cpp
        src/core/datasets/m3fd_adapter.h

        src/core/datasets/tno_adapter.cpp
        src/core/datasets/tno_adapter.h

        src/core/visualization/preview_selection.cpp
        src/core/visualization/preview_selection.h

        src/core/visualization/preview_pair_selector.cpp
        src/core/visualization/preview_pair_selector.h

        src/core/visualization/preview_renderer.cpp
        src/core/visualization/preview_renderer.h

        src/core/visualization/opencv_preview_renderer.cpp
        src/core/visualization/opencv_preview_renderer.h

        src/core/enhancement/thermal_input.cpp
        src/core/enhancement/thermal_input.h

        src/core/enhancement/n5_thermal_enhancer.cpp
        src/core/enhancement/n5_thermal_enhancer.h
)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set_property(
            SOURCE src/core/enhancement/n5_thermal_enhancer.cpp
            APPEND PROPERTY COMPILE_OPTIONS
            -ffp-contract=off
            -fno-fast-math
    )
elseif(MSVC)
    set_property(
            SOURCE src/core/enhancement/n5_thermal_enhancer.cpp
            APPEND PROPERTY COMPILE_OPTIONS
            /fp:strict
    )
endif()

target_include_directories(qart_core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}

#        PRIVATE
#        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(qart_core
        PRIVATE
            ${OpenCV_LIBS}
            nlohmann_json::nlohmann_json
)


# ---------------------------------------------------------------------------
# Qt application shell
# ---------------------------------------------------------------------------

qt_add_executable(Visual_Thermal_Concept
        WIN32 MACOSX_BUNDLE
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui

        src/application/application_controller.cpp
        src/application/application_controller.h
)

target_include_directories(Visual_Thermal_Concept
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Visual_Thermal_Concept
        PRIVATE
        qart_core
        Qt::Core
        Qt::Widgets
)


# ---------------------------------------------------------------------------
# Contract tests
# ---------------------------------------------------------------------------

include(CTest)

if(BUILD_TESTING)

    add_executable(test_dataset_partition_id
            tests/domain/datasets/test_dataset_partition_id.cpp
    )

    target_link_libraries(test_dataset_partition_id
            PRIVATE
            qart_core
    )

    add_test(
            NAME DatasetPartitionIdContract
            COMMAND test_dataset_partition_id
    )


    add_executable(test_dataset_usage_role
            tests/domain/datasets/test_dataset_usage_role.cpp
    )

    target_link_libraries(test_dataset_usage_role
            PRIVATE
            qart_core
    )

    add_test(
            NAME DatasetUsageRoleContract
            COMMAND test_dataset_usage_role
    )


    add_executable(test_experiment_view_assignment
            tests/domain/datasets/test_experiment_view_assignment.cpp
    )

    target_link_libraries(test_experiment_view_assignment
            PRIVATE
            qart_core
    )

    add_test(
            NAME ExperimentViewAssignmentContract
            COMMAND test_experiment_view_assignment
    )


    add_executable(test_experiment_view
            tests/domain/datasets/test_experiment_view.cpp
    )

    target_link_libraries(test_experiment_view
            PRIVATE
            qart_core
    )

    add_test(
            NAME ExperimentViewContract
            COMMAND test_experiment_view
    )

    add_executable(test_image_dimensions
            tests/domain/imaging/test_image_dimensions.cpp
    )

    target_link_libraries(test_image_dimensions
            PRIVATE
            qart_core
    )

    add_test(
            NAME ImageDimensionsContract
            COMMAND test_image_dimensions
    )

    add_executable(test_dataset_pair
            tests/domain/datasets/test_dataset_pair.cpp
    )

    target_link_libraries(test_dataset_pair
            PRIVATE
            qart_core
    )

    add_test(
            NAME DatasetPairContract
            COMMAND test_dataset_pair
    )

    add_executable(test_pair_validation_status
            tests/domain/validation/test_pair_validation_status.cpp
    )

    target_link_libraries(test_pair_validation_status
            PRIVATE
            qart_core
    )

    add_test(
            NAME PairValidationStatusContract
            COMMAND test_pair_validation_status
    )

    add_executable(test_pair_validation_result
            tests/domain/validation/test_pair_validation_result.cpp
    )

    target_link_libraries(test_pair_validation_result
            PRIVATE
            qart_core
    )

    add_test(
            NAME PairValidationResultContract
            COMMAND test_pair_validation_result
    )

    add_executable(test_image_decode_status
            tests/domain/imaging/test_image_decode_status.cpp
    )

    target_link_libraries(test_image_decode_status
            PRIVATE
            qart_core
    )

    add_test(
            NAME ImageDecodeStatusContract
            COMMAND test_image_decode_status
    )

    add_executable(test_image_decode_result
            tests/domain/imaging/test_image_decode_result.cpp
    )

    target_link_libraries(test_image_decode_result
            PRIVATE
            qart_core
    )

    add_test(
            NAME ImageDecodeResultContract
            COMMAND test_image_decode_result
    )

    add_executable(test_image_decoder
            tests/imaging/test_image_decoder.cpp
    )

    target_link_libraries(test_image_decoder
            PRIVATE
            qart_core
    )

    add_test(
            NAME ImageDecoderContract
            COMMAND test_image_decoder
    )

    add_executable(test_opencv_image_decoder
            tests/imaging/test_opencv_image_decoder.cpp
    )

    target_include_directories(test_opencv_image_decoder
            PRIVATE
            ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(test_opencv_image_decoder
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_test(
            NAME OpenCvImageDecoderContract
            COMMAND test_opencv_image_decoder
    )

    add_executable(test_pair_validation_policy
            tests/domain/validation/test_pair_validation_policy.cpp
    )

    target_link_libraries(test_pair_validation_policy
            PRIVATE
            qart_core
    )

    add_test(
            NAME PairValidationPolicyContract
            COMMAND test_pair_validation_policy
    )

    add_executable(test_dataset_exceptions
            tests/domain/datasets/test_dataset_exceptions.cpp
    )

    target_link_libraries(test_dataset_exceptions
            PRIVATE
            qart_core
    )

    add_test(
            NAME DatasetExceptionsContract
            COMMAND test_dataset_exceptions
    )

    add_executable(test_pair_validator
            tests/validation/test_pair_validator.cpp
    )

    target_link_libraries(test_pair_validator
            PRIVATE
            qart_core
    )

    add_test(
            NAME PairValidatorContract
            COMMAND test_pair_validator
    )

    add_executable(test_validation_status_count
            tests/domain/validation/test_validation_status_count.cpp
    )

    target_link_libraries(test_validation_status_count
            PRIVATE
            qart_core
    )

    add_test(
            NAME ValidationStatusCountContract
            COMMAND test_validation_status_count
    )

    add_executable(test_batch_validation_summary
            tests/domain/validation/test_batch_validation_summary.cpp
    )

    target_link_libraries(test_batch_validation_summary
            PRIVATE
            qart_core
    )

    add_test(
            NAME BatchValidationSummaryContract
            COMMAND test_batch_validation_summary
    )

    add_executable(test_batch_validation_report
            tests/domain/validation/test_batch_validation_report.cpp
    )

    target_link_libraries(test_batch_validation_report
            PRIVATE
            qart_core
    )

    add_test(
            NAME BatchValidationReportContract
            COMMAND test_batch_validation_report
    )

    add_executable(test_batch_validation_progress
            tests/domain/validation/test_batch_validation_progress.cpp
    )

    target_link_libraries(test_batch_validation_progress
            PRIVATE
            qart_core
    )

    add_test(
            NAME BatchValidationProgressContract
            COMMAND test_batch_validation_progress
    )

    add_executable(test_batch_pair_validator
            tests/validation/test_batch_pair_validator.cpp
    )

    target_link_libraries(test_batch_pair_validator
            PRIVATE
            qart_core
    )

    add_test(
            NAME BatchPairValidatorContract
            COMMAND test_batch_pair_validator
    )

    add_executable(test_dataset_configuration
            tests/configuration/test_dataset_configuration.cpp
    )

    target_link_libraries(test_dataset_configuration
            PRIVATE
            qart_core
    )

    add_test(
            NAME DatasetConfigurationContract
            COMMAND test_dataset_configuration
    )

    add_executable(test_json_dataset_configuration_loader
            tests/configuration/test_json_dataset_configuration_loader.cpp
    )

    target_link_libraries(test_json_dataset_configuration_loader
            PRIVATE
            qart_core
    )

    add_test(
            NAME JsonDatasetConfigurationLoaderContract
            COMMAND test_json_dataset_configuration_loader
    )

    add_executable(test_json_experiment_view_loader
            tests/configuration/test_json_experiment_view_loader.cpp
    )

    target_link_libraries(test_json_experiment_view_loader
            PRIVATE
            qart_core
    )

    add_test(
            NAME JsonExperimentViewLoaderContract
            COMMAND test_json_experiment_view_loader
    )

    add_executable(test_validation_manifest_row
            tests/manifests/test_validation_manifest_row.cpp
    )

    target_link_libraries(test_validation_manifest_row
            PRIVATE
            qart_core
    )

    add_test(
            NAME ValidationManifestRowContract
            COMMAND test_validation_manifest_row
    )

    add_executable(test_validation_manifest
            tests/manifests/test_validation_manifest.cpp
    )

    target_link_libraries(test_validation_manifest
            PRIVATE
            qart_core
    )

    add_test(
            NAME ValidationManifestContract
            COMMAND test_validation_manifest
    )

    add_executable(test_csv_validation_manifest_reader
            tests/manifests/test_csv_validation_manifest_reader.cpp
    )

    target_link_libraries(test_csv_validation_manifest_reader
            PRIVATE
            qart_core
    )

    add_test(
            NAME CsvValidationManifestReaderContract
            COMMAND test_csv_validation_manifest_reader
    )

    add_executable(test_csv_validation_manifest_writer
            tests/manifests/test_csv_validation_manifest_writer.cpp
    )

    target_link_libraries(test_csv_validation_manifest_writer
            PRIVATE
            qart_core
    )

    add_test(
            NAME CsvValidationManifestWriterContract
            COMMAND test_csv_validation_manifest_writer
    )

    add_executable(test_dataset_adapter
            tests/datasets/test_dataset_adapter.cpp
    )

    target_link_libraries(test_dataset_adapter
            PRIVATE
            qart_core
    )

    add_test(
            NAME DatasetAdapterContract
            COMMAND test_dataset_adapter
    )

    add_executable(test_canonical_pair_lookup
            tests/datasets/test_canonical_pair_lookup.cpp
    )

    target_link_libraries(test_canonical_pair_lookup
            PRIVATE
            qart_core
    )

    add_test(
            NAME CanonicalPairLookupContract
            COMMAND test_canonical_pair_lookup
    )

    add_executable(test_llvip_adapter
            tests/datasets/test_llvip_adapter.cpp
    )

    target_link_libraries(test_llvip_adapter
            PRIVATE
            qart_core
    )

    add_test(
            NAME LLVIPAdapterContract
            COMMAND test_llvip_adapter
    )

    add_executable(test_msrs_adapter
            tests/datasets/test_msrs_adapter.cpp
    )

    target_link_libraries(test_msrs_adapter
            PRIVATE
            qart_core
    )

    add_test(
            NAME MSRSAdapterContract
            COMMAND test_msrs_adapter
    )

    add_executable(test_roadscene_adapter
            tests/datasets/test_roadscene_adapter.cpp
    )

    target_link_libraries(test_roadscene_adapter
            PRIVATE
            qart_core
    )

    add_test(
            NAME RoadSceneAdapterContract
            COMMAND test_roadscene_adapter
    )

    add_executable(
            test_m3fd_adapter
            tests/datasets/test_m3fd_adapter.cpp
    )

    target_link_libraries(
            test_m3fd_adapter
            PRIVATE
            qart_core
    )

    add_test(
            NAME M3FDAdapterContract
            COMMAND test_m3fd_adapter
    )


    add_executable(
            test_tno_adapter
            tests/datasets/test_tno_adapter.cpp
    )

    target_link_libraries(
            test_tno_adapter
            PRIVATE
            qart_core
    )

    add_test(
            NAME TNOAdapterContract
            COMMAND test_tno_adapter
    )

    add_executable(test_preview_selection
            tests/visualization/test_preview_selection.cpp
    )

    target_link_libraries(test_preview_selection
            PRIVATE
            qart_core
    )

    add_test(
            NAME PreviewSelectionContract
            COMMAND test_preview_selection
    )

    add_executable(test_preview_pair_selector
            tests/visualization/test_preview_pair_selector.cpp
    )

    target_link_libraries(test_preview_pair_selector
            PRIVATE
            qart_core
    )

    add_test(
            NAME PreviewPairSelectorContract
            COMMAND test_preview_pair_selector
    )

    add_executable(test_preview_renderer
            tests/visualization/test_preview_renderer.cpp
    )

    target_link_libraries(test_preview_renderer
            PRIVATE
            qart_core
    )

    add_test(
            NAME PreviewRendererContract
            COMMAND test_preview_renderer
    )

    add_executable(test_opencv_preview_renderer
            tests/visualization/test_opencv_preview_renderer.cpp
    )

    target_include_directories(test_opencv_preview_renderer
            PRIVATE
            ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(test_opencv_preview_renderer
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_test(
            NAME OpenCvPreviewRendererContract
            COMMAND test_opencv_preview_renderer
    )

    add_executable(test_thermal_input
            tests/enhancement/test_thermal_input.cpp
    )

    target_link_libraries(test_thermal_input
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_test(
            NAME ThermalInputContract
            COMMAND test_thermal_input
    )

    set_tests_properties(ThermalInputContract
            PROPERTIES LABELS "phase2;thermal_input"
    )

    add_executable(test_phase2_thermal_input_parity
            tests/parity/test_phase2_thermal_input_parity.cpp
    )

    target_link_libraries(test_phase2_thermal_input_parity
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_executable(test_n5_thermal_enhancer
            tests/enhancement/test_n5_thermal_enhancer.cpp
    )

    target_link_libraries(test_n5_thermal_enhancer
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_test(
            NAME N5ThermalContract
            COMMAND test_n5_thermal_enhancer
    )

    set_tests_properties(N5ThermalContract
            PROPERTIES LABELS "phase2;enhancement;n5"
    )

    add_executable(test_phase2_n5_parity
            tests/parity/test_phase2_n5_parity.cpp
    )

    target_link_libraries(test_phase2_n5_parity
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_executable(test_application_thermal_processing
            tests/application/test_application_thermal_processing.cpp
            src/application/application_controller.cpp
    )

    target_link_libraries(test_application_thermal_processing
            PRIVATE
            qart_core
            ${OpenCV_LIBS}
    )

    add_test(
            NAME ApplicationThermalProcessingContract
            COMMAND test_application_thermal_processing
    )

    set_tests_properties(ApplicationThermalProcessingContract
            PROPERTIES LABELS "phase2;application;enhancement"
    )

    if(QART_PHASE2_N5_FIXTURE_DIR)
        if(NOT EXISTS "${QART_PHASE2_N5_FIXTURE_DIR}/cases.txt")
            message(FATAL_ERROR
                    "N5 fixture directory has no cases.txt: "
                    "${QART_PHASE2_N5_FIXTURE_DIR}"
            )
        endif()

        add_test(
                NAME Phase2N5Parity
                COMMAND test_phase2_n5_parity
                "${QART_PHASE2_N5_FIXTURE_DIR}"
        )

        set_tests_properties(Phase2N5Parity
                PROPERTIES LABELS "phase2;enhancement;n5;parity"
        )
    else()
        message(STATUS
                "Phase2N5Parity not registered: "
                "set QART_PHASE2_N5_FIXTURE_DIR to enable it."
        )
    endif()

    if(QART_PHASE2_N5_REAL_FIXTURE_DIR)
        if(NOT EXISTS "${QART_PHASE2_N5_REAL_FIXTURE_DIR}/cases.txt")
            message(FATAL_ERROR
                    "Native N5 fixture directory has no cases.txt: "
                    "${QART_PHASE2_N5_REAL_FIXTURE_DIR}"
            )
        endif()

        add_test(
                NAME Phase2N5RealParity
                COMMAND test_phase2_n5_parity
                "${QART_PHASE2_N5_REAL_FIXTURE_DIR}"
        )

        set_tests_properties(Phase2N5RealParity
                PROPERTIES LABELS "phase2;enhancement;n5;parity;native"
        )
    endif()

    if(QART_PHASE2_INPUT_FIXTURE_DIR)
        if(NOT EXISTS "${QART_PHASE2_INPUT_FIXTURE_DIR}/cases.txt")
            message(FATAL_ERROR
                    "Phase 2 fixture directory has no cases.txt: "
                    "${QART_PHASE2_INPUT_FIXTURE_DIR}"
            )
        endif()

        add_test(
                NAME Phase2ThermalInputParity
                COMMAND test_phase2_thermal_input_parity
                "${QART_PHASE2_INPUT_FIXTURE_DIR}"
        )

        set_tests_properties(Phase2ThermalInputParity
                PROPERTIES LABELS "phase2;thermal_input;parity"
        )
    else()
        message(STATUS
                "Phase2ThermalInputParity not registered: "
                "set QART_PHASE2_INPUT_FIXTURE_DIR to enable it."
        )
    endif()

    if(QART_ENABLE_REAL_DATA_TESTS)

        if(NOT QART_REAL_DATA_ROOT)
            message(
                    FATAL_ERROR
                    "QART_ENABLE_REAL_DATA_TESTS is ON but "
                    "QART_REAL_DATA_ROOT is empty."
            )
        endif()

        add_executable(
                test_llvip_real_parity
                tests/parity/test_llvip_real_parity.cpp
        )

        target_link_libraries(
                test_llvip_real_parity
                PRIVATE
                qart_core
        )

        add_test(
                NAME LLVIPRealDataParity
                COMMAND
                test_llvip_real_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                LLVIPRealDataParity
                PROPERTIES
                LABELS "phase1;parity;real_data;llvip"
        )

        add_executable(
                test_llvip_real_preview_parity
                tests/parity/test_llvip_real_preview_parity.cpp
        )

        target_include_directories(
                test_llvip_real_preview_parity
                PRIVATE
                ${OpenCV_INCLUDE_DIRS}
        )

        target_link_libraries(
                test_llvip_real_preview_parity
                PRIVATE
                qart_core
                ${OpenCV_LIBS}
        )

        add_test(
                NAME LLVIPRealPreviewParity
                COMMAND
                test_llvip_real_preview_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                LLVIPRealPreviewParity
                PROPERTIES
                LABELS "phase1;parity;real_data;llvip;preview"
        )

        add_executable(
                test_msrs_real_parity
                tests/parity/test_msrs_real_parity.cpp
        )

        target_link_libraries(
                test_msrs_real_parity
                PRIVATE
                qart_core
        )

        add_test(
                NAME MSRSRealDataParity
                COMMAND
                test_msrs_real_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                MSRSRealDataParity
                PROPERTIES
                LABELS "phase1;parity;real_data;msrs"
        )

        add_executable(
                test_msrs_real_preview_parity
                tests/parity/test_msrs_real_preview_parity.cpp
        )

        target_include_directories(
                test_msrs_real_preview_parity
                PRIVATE
                ${OpenCV_INCLUDE_DIRS}
        )

        target_link_libraries(
                test_msrs_real_preview_parity
                PRIVATE
                qart_core
                ${OpenCV_LIBS}
        )

        add_test(
                NAME MSRSRealPreviewParity
                COMMAND
                test_msrs_real_preview_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                MSRSRealPreviewParity
                PROPERTIES
                LABELS "phase1;parity;real_data;msrs;preview"
        )

        add_executable(
                test_roadscene_real_parity
                tests/parity/test_roadscene_real_parity.cpp
        )

        target_link_libraries(
                test_roadscene_real_parity
                PRIVATE
                qart_core
        )

        add_test(
                NAME RoadSceneRealDataParity
                COMMAND
                test_roadscene_real_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                RoadSceneRealDataParity
                PROPERTIES
                LABELS "phase1;parity;real_data;roadscene"
        )

        add_executable(
                test_roadscene_real_preview_parity
                tests/parity/test_roadscene_real_preview_parity.cpp
        )

        target_include_directories(
                test_roadscene_real_preview_parity
                PRIVATE
                ${OpenCV_INCLUDE_DIRS}
        )

        target_link_libraries(
                test_roadscene_real_preview_parity
                PRIVATE
                qart_core
                ${OpenCV_LIBS}
        )

        add_test(
                NAME RoadSceneRealPreviewParity
                COMMAND
                test_roadscene_real_preview_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                RoadSceneRealPreviewParity
                PROPERTIES
                LABELS "phase1;parity;real_data;roadscene;preview"
        )
        add_executable(
                test_m3fd_real_parity
                tests/parity/test_m3fd_real_parity.cpp
        )

        target_link_libraries(
                test_m3fd_real_parity
                PRIVATE
                qart_core
        )

        add_test(
                NAME M3FDRealDataParity
                COMMAND
                test_m3fd_real_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                M3FDRealDataParity
                PROPERTIES
                LABELS "phase1;parity;real_data;m3fd"
        )
        add_executable(
                test_m3fd_real_preview_parity
                tests/parity/test_m3fd_real_preview_parity.cpp
        )

        target_include_directories(
                test_m3fd_real_preview_parity
                PRIVATE
                ${OpenCV_INCLUDE_DIRS}
        )

        target_link_libraries(
                test_m3fd_real_preview_parity
                PRIVATE
                qart_core
                ${OpenCV_LIBS}
        )

        add_test(
                NAME M3FDRealPreviewParity
                COMMAND
                test_m3fd_real_preview_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                M3FDRealPreviewParity
                PROPERTIES
                LABELS "phase1;parity;real_data;m3fd;preview"
        )
        add_executable(
                test_tno_real_parity
                tests/parity/test_tno_real_parity.cpp
        )

        target_link_libraries(
                test_tno_real_parity
                PRIVATE
                qart_core
        )

        add_test(
                NAME TNORealDataParity
                COMMAND
                test_tno_real_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                TNORealDataParity
                PROPERTIES
                LABELS "phase1;parity;real_data;tno"
        )
        add_executable(
                test_tno_real_preview_parity
                tests/parity/test_tno_real_preview_parity.cpp
        )

        target_include_directories(
                test_tno_real_preview_parity
                PRIVATE
                ${OpenCV_INCLUDE_DIRS}
        )

        target_link_libraries(
                test_tno_real_preview_parity
                PRIVATE
                qart_core
                ${OpenCV_LIBS}
        )

        add_test(
                NAME TNORealPreviewParity
                COMMAND
                test_tno_real_preview_parity
                "${QART_REAL_DATA_ROOT}"
        )

        set_tests_properties(
                TNORealPreviewParity
                PROPERTIES
                LABELS "phase1;parity;real_data;tno;preview"
        )
    endif()
endif()


# ---------------------------------------------------------------------------
# Deployment
# ---------------------------------------------------------------------------

install(TARGETS Visual_Thermal_Concept
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
        TARGET Visual_Thermal_Concept
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
