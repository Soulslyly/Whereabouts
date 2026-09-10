add_custom_target(
    stage_release
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${PROJECT_SOURCE_DIR}/tools/stage-release.ps1"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

add_custom_target(
    validate_release
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${PROJECT_SOURCE_DIR}/tools/validate-release.ps1"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
