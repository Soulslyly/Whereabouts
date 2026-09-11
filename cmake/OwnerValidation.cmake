if(NOT DEFINED WHEREABOUTS_OWNER_VALIDATION)
    set(WHEREABOUTS_OWNER_VALIDATION OFF)
endif()

if(WHEREABOUTS_OWNER_VALIDATION)
    if(NOT DEFINED WHEREABOUTS_SMF_DLL OR WHEREABOUTS_SMF_DLL STREQUAL "")
        message(FATAL_ERROR "WHEREABOUTS_SMF_DLL is required when WHEREABOUTS_OWNER_VALIDATION is ON")
    endif()
    if(NOT EXISTS "${WHEREABOUTS_SMF_DLL}" OR IS_DIRECTORY "${WHEREABOUTS_SMF_DLL}")
        message(FATAL_ERROR "WHEREABOUTS_SMF_DLL must name the exact installed SKSEMenuFramework.dll")
    endif()
    foreach(required_path WHEREABOUTS_SKYRIM_1170_EXECUTABLE WHEREABOUTS_SKSE_226_DLL)
        if(NOT DEFINED ${required_path} OR "${${required_path}}" STREQUAL "")
            message(FATAL_ERROR "${required_path} is required when WHEREABOUTS_OWNER_VALIDATION is ON")
        endif()
        if(NOT EXISTS "${${required_path}}" OR IS_DIRECTORY "${${required_path}}")
            message(FATAL_ERROR "${required_path} must name an exact installed runtime file")
        endif()
    endforeach()

    get_filename_component(WHEREABOUTS_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    execute_process(
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${WHEREABOUTS_PROJECT_ROOT}/tools/validate-smf-binary.ps1"
            -MenuFrameworkDll "${WHEREABOUTS_SMF_DLL}"
            -ProjectRoot "${WHEREABOUTS_PROJECT_ROOT}"
        RESULT_VARIABLE WHEREABOUTS_SMF_VALIDATION_RESULT
        OUTPUT_VARIABLE WHEREABOUTS_SMF_VALIDATION_OUTPUT
        ERROR_VARIABLE WHEREABOUTS_SMF_VALIDATION_ERROR
    )
    if(NOT WHEREABOUTS_SMF_VALIDATION_RESULT EQUAL 0)
        message(FATAL_ERROR "Exact SMF binary validation failed:\n${WHEREABOUTS_SMF_VALIDATION_OUTPUT}${WHEREABOUTS_SMF_VALIDATION_ERROR}")
    endif()

    execute_process(
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${WHEREABOUTS_PROJECT_ROOT}/tools/validate-runtime-dependency.ps1"
            -SkyrimExecutable "${WHEREABOUTS_SKYRIM_1170_EXECUTABLE}"
            -SkseDll "${WHEREABOUTS_SKSE_226_DLL}"
            -ExpectedSkyrimFileVersion "1.6.1170.0"
            -ExpectedSkseFileVersion "0.2.2.6"
        RESULT_VARIABLE WHEREABOUTS_RUNTIME_VALIDATION_RESULT
        OUTPUT_VARIABLE WHEREABOUTS_RUNTIME_VALIDATION_OUTPUT
        ERROR_VARIABLE WHEREABOUTS_RUNTIME_VALIDATION_ERROR
    )
    if(NOT WHEREABOUTS_RUNTIME_VALIDATION_RESULT EQUAL 0)
        message(FATAL_ERROR "Exact Skyrim/SKSE dependency validation failed:\n${WHEREABOUTS_RUNTIME_VALIDATION_OUTPUT}${WHEREABOUTS_RUNTIME_VALIDATION_ERROR}")
    endif()
endif()
