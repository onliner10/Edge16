include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SRC ${SRC} storage/yaml/yaml_tree_walker.cpp
               storage/yaml/yaml_parser.cpp
               storage/yaml/yaml_bits.cpp
               storage/yaml/yaml_datastructs.cpp)

if(STORAGE_MODELSLIST)
  set(SRC ${SRC} storage/yaml/yaml_labelslist.cpp)
  set(SRC ${SRC} storage/yaml/yaml_modelslist.cpp)
  add_definitions(-DSTORAGE_MODELSLIST)
endif()

# Warning: this file must be kept in sync with 'yaml_datastructs.cpp'
#          in the same directory.

if((PCB STREQUAL X10) AND (PCBREV STREQUAL TX16S))
  set(YAML_GEN_OUTPUT storage/yaml/yaml_datastructs_x10.cpp)
elseif(PCB STREQUAL TX16SMK3)
  set(YAML_GEN_OUTPUT storage/yaml/yaml_datastructs_tx16smk3.cpp)
else()
  message(FATAL_ERROR "Only TX16S MK2 and TX16S MK3 are supported by Edge16 YAML storage (PCB='${PCB}', PCBREV='${PCBREV}')")
endif()

set(YAML_GEN          ${RADIO_DIRECTORY}/util/generate_yaml.py)
set(YAML_GEN_TEMPLATE ${RADIO_DIRECTORY}/util/yaml_parser.tmpl)

SET(YAML_NODES        "\"RadioData,ModelData,PartialModel\"")
set(YAML_GEN_ARGS     myeeprom.h ${YAML_GEN_TEMPLATE} ${YAML_NODES} -DYAML_GENERATOR)

AddCompilerFlags(YAML_GEN_ARGS)

# Force to build 32 bit binaries (Fix for Apple Silicon M1 hosts)
# TODO: find a way to detect APPLE & M1
# set(YAML_GEN_ARGS ${YAML_GEN_ARGS} -m32)

set(YAML_GEN_ARGS ${YAML_GEN_ARGS}
  -Wno-inconsistent-missing-override
  -Wno-asm-operand-widths
  ${SYSROOT_ARG})

add_custom_target(yaml_data
    WORKING_DIRECTORY ${RADIO_DIRECTORY}/src
    COMMAND ${PYTHON_EXECUTABLE} ${YAML_GEN} ${YAML_GEN_ARGS} > ${YAML_GEN_OUTPUT}
    DEPENDS ${RADIO_DIRECTORY}/src/datastructs.h
            ${RADIO_DIRECTORY}/src/dataconstants.h
            ${RADIO_DIRECTORY}/src/myeeprom.h
            ${RADIO_DIRECTORY}/util/generate_yaml.py
)

AddHWGenTarget(${HW_DESC_JSON} yaml_inputs yaml_inputs.inc)
set(SRC ${SRC} ${CMAKE_CURRENT_BINARY_DIR}/yaml_inputs.inc)

# helper target for testing purposes
add_custom_target(yaml_inputs DEPENDS ${HW_DESC_JSON} yaml_inputs.inc)

