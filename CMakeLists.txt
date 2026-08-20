cmake_minimum_required(VERSION 3.19)

project(Visual_Thermal_Concept LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc)
find_package(nlohmann_json CONFIG REQUIRED)

qt_standard_project_setup()


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

        src/core/datasets/llvip_adapter.cpp
        src/core/datasets/llvip_adapter.h

        src/core/datasets/msrs_adapter.cpp
        src/core/datasets/msrs_adapter.h

        src/core/datasets/roadscene_adapter.cpp
        src/core/datasets/roadscene_adapter.h

        src/core/visualization/preview_selection.cpp
        src/core/visualization/preview_selection.h

        src/core/visualization/preview_pair_selector.cpp
        src/core/visualization/preview_pair_selector.h

        src/core/visualization/preview_renderer.cpp
        src/core/visualization/preview_renderer.h

        src/core/visualization/opencv_preview_renderer.cpp
        src/core/visualization/opencv_preview_renderer.h
)

target_include_directories(qart_core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src

        PRIVATE
        ${OpenCV_INCLUDE_DIRS}
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
)

target_link_libraries(Visual_Thermal_Concept
        PRIVATE
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