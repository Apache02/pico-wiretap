function(add_deploy_target TARGET PATH_UF2)
    add_custom_target(
            "${TARGET}---deploy"
            DEPENDS ${TARGET}
            DEPENDS ${PATH_UF2}
            COMMAND PICO_DEPLOY_TTY=${PICO_DEPLOY_TTY} PICO_DEPLOY_TARGET=${PICO_DEPLOY_TARGET} ${CMAKE_SOURCE_DIR}/scripts/deploy.sh ${PATH_UF2}
    )
endfunction()

