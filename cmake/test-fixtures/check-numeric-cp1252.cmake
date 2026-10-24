# Runs ExtractStringTable.cmake on numeric-cp1252-stringtable.rc with
# RC_ENCODING=CP1252 and checks the generated literals: numeric IDs need no
# header, a cp1252 byte becomes an octal escape and ';' survives the list.
# Inputs: EXTRACTOR, FIXTURE_DIR, OUTPUT_CPP.
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_CPP=${OUTPUT_CPP}
        -DRC_FILE=${FIXTURE_DIR}/numeric-cp1252-stringtable.rc
        -DRC_ENCODING=CP1252
        -DRESOURCE_HEADERS=${FIXTURE_DIR}/minimal-resource.h
        -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=test-fixture
        -DVERIFY_ID=7
        -P ${EXTRACTOR}
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "extraction failed (${rc})")
endif()
file(READ ${OUTPUT_CPP} generated)
foreach(expected "{7u, \"Caf\\351\"}," "{8u, \"a\\073b\"},")
    string(FIND "${generated}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "generated table lacks ${expected}:\n${generated}")
    endif()
endforeach()
