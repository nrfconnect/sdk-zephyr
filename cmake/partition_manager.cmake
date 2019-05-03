#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

define_property(GLOBAL PROPERTY PM_IMAGES
  BRIEF_DOCS "A list of all images (${IMAGE} instances) that should be processed by the Partition Manager."
  FULL_DOCS "A list of all images (${IMAGE} instances) that should be processed by the Partition Manager.
Each image's directory will be searched for a pm.yml, and will receive a pm_config.h header file with the result.
Also, the each image's hex file will be automatically associated with its partition.")

if(FIRST_BOILERPLATE_EXECUTION)
  get_property(PM_IMAGES GLOBAL PROPERTY PM_IMAGES)

  if(PM_IMAGES)
    # Partition manager is enabled because we have populated PM_IMAGES.

    set(generated_path include/generated)

    # Add the "app" as an image partition.
    set_property(GLOBAL PROPERTY app_PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR})
    set_property(GLOBAL PROPERTY app_PM_HEX_FILE ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME})
    set_property(GLOBAL PROPERTY app_PM_TARGET ${logical_target_for_zephyr_elf})
    list(APPEND PM_IMAGES app_)

    # Prepare the input_files, header_files, and images lists
    foreach (IMAGE ${PM_IMAGES})
      get_image_name(${IMAGE} image_name) # Removes the trailing '_'
      list(APPEND images ${image_name})
      get_property(${IMAGE}PROJECT_BINARY_DIR GLOBAL PROPERTY ${IMAGE}PROJECT_BINARY_DIR)
      list(APPEND input_files ${${IMAGE}PROJECT_BINARY_DIR}/${generated_path}/pm.yml)
      list(APPEND header_files ${${IMAGE}PROJECT_BINARY_DIR}/${generated_path}/pm_config.h)
    endforeach()

    math(EXPR flash_size "${CONFIG_FLASH_SIZE} * 1024")

    set(pm_cmd
      ${PYTHON_EXECUTABLE}
      ${ZEPHYR_BASE}/scripts/partition_manager.py
      --input-names ${images}
      --input-files ${input_files}
      --flash-size ${flash_size}
      --output ${CMAKE_BINARY_DIR}/partitions.yml
      )

    set(pm_output_cmd
      ${PYTHON_EXECUTABLE}
      ${ZEPHYR_BASE}/scripts/partition_manager_output.py
      --input ${CMAKE_BINARY_DIR}/partitions.yml
      --config-file ${CMAKE_BINARY_DIR}/pm.config
      --input-names ${images}
      --header-files
      ${header_files}
      )

    # Run the partition manager algorithm.
    execute_process(
      COMMAND
      ${pm_cmd}
      RESULT_VARIABLE ret
      )

    if(NOT ${ret} EQUAL "0")
      message(FATAL_ERROR "Partition Manager failed, aborting. Command: ${pm_cmd}")
    endif()

    # Produce header files and config file.
    execute_process(
      COMMAND
      ${pm_output_cmd}
      RESULT_VARIABLE ret
      )

    if(NOT ${ret} EQUAL "0")
      message(FATAL_ERROR "Partition Manager output generation failed, aborting. Command: ${pm_output_cmd}")
    endif()

    # Make Partition Manager configuration available in CMake
    import_kconfig(PM_ ${CMAKE_BINARY_DIR}/pm.config)

    # Create a dummy target that we can add properties to for
    # extraction in generator expressions.
    add_custom_target(partition_manager)

    set_property(
      TARGET partition_manager
      PROPERTY MCUBOOT_SLOT_SIZE
      ${PM_MCUBOOT_PRIMARY_SIZE}
      )
    set_property(
      TARGET partition_manager
      PROPERTY MCUBOOT_HEADER_SIZE
      ${PM_MCUBOOT_PAD_SIZE}
      )
    set_property(
      TARGET partition_manager
      PROPERTY MCUBOOT_SECONDARY_ADDRESS
      ${PM_MCUBOOT_SECONDARY_ADDRESS}
      )

    get_property(
      hex_files_to_merge
      GLOBAL PROPERTY
      PM_HEX_FILES_TO_MERGE
      )
    if (hex_files_to_merge AND NOT PM_MCUBOOT_ADDRESS)
      # Unfortunately, we can not re-use the merging functionality of the
      # zephyr build system when using the partition manager. This because it
      # is not known until all sub-images has been included whether or not
      # some special handling is required (e.g. with mcuboot).

      get_property(
        hex_files_to_merge_targets
        GLOBAL PROPERTY
        PM_HEX_FILES_TO_MERGE_TARGETS
        )

      set(merged_hex ${PROJECT_BINARY_DIR}/merged.hex)
      add_custom_command(
        OUTPUT ${merged_hex}
        COMMAND
        ${PYTHON_EXECUTABLE}
        ${ZEPHYR_BASE}/scripts/mergehex.py
        -o ${PROJECT_BINARY_DIR}/merged.hex
        ${hex_files_to_merge}
        ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
        DEPENDS
        ${hex_files_to_merge_targets}
        zephyr_final
        )
      add_custom_target(pm_mergehex ALL DEPENDS ${merged_hex})

      if(TARGET flash)
        add_dependencies(flash pm_mergehex)
      endif()

      set(ZEPHYR_RUNNER_CONFIG_KERNEL_HEX "${merged_hex}"
        CACHE STRING "Path to merged image in Intel Hex format" FORCE)

    elseif (PM_SPM_ADDRESS AND PM_MCUBOOT_ADDRESS)
      # Special handling needed to merge before signing.
      set(merged_to_sign_hex ${PROJECT_BINARY_DIR}/merged_to_sign.hex)
      add_custom_command(
        OUTPUT ${merged_to_sign_hex}
        COMMAND
        ${PYTHON_EXECUTABLE}
        ${ZEPHYR_BASE}/scripts/mergehex.py
        -o ${merged_to_sign_hex}
        ${spm_PROJECT_BINARY_DIR}/zephyr.hex
        ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
        DEPENDS
        spm_zephyr_final
        zephyr_final
        )
      add_custom_target(merged_to_sign_target DEPENDS ${merged_to_sign_hex})

      set_property(
        TARGET partition_manager
        PROPERTY MCUBOOT_TO_SIGN
        ${merged_to_sign_hex}
        )
      set_property(
        TARGET partition_manager
        PROPERTY MCUBOOT_TO_SIGN_DEPENDS
        merged_to_sign_target
        ${merged_to_sign_hex}
        )
    endif()
  endif()
endif()
