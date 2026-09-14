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

add_custom_target(
    stage_github_source
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${PROJECT_SOURCE_DIR}/tools/stage-github-source.ps1"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

add_custom_target(
    validate_github_source
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File "${PROJECT_SOURCE_DIR}/tools/validate-github-source.ps1"
        -SourceRoot "${PROJECT_SOURCE_DIR}/staging/github-source"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
