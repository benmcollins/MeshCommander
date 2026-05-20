# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

function(qumesh_wsman_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
        qumesh_wsman
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qumesh_wsman_test(test_soap_envelope)
qumesh_wsman_test(test_local_amt_probe)
qumesh_wsman_test(test_event_log_decoder)
qumesh_wsman_test(test_setupbin)
qumesh_wsman_test(test_cert_request_builder)
target_link_libraries(test_cert_request_builder PRIVATE OpenSSL::Crypto)
qumesh_wsman_test(test_ocr_tlv)

if(TARGET Qt6::HttpServer AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_wsman_client.cpp)
    qumesh_wsman_test(test_wsman_client)
    target_link_libraries(test_wsman_client PRIVATE Qt6::HttpServer)
elseif(NOT TARGET Qt6::HttpServer)
    message(WARNING "Qt6::HttpServer not found — skipping test_wsman_client. "
                    "Install qthttpserver to enable mock-server integration tests.")
endif()

# Trust-on-first-use → retry path (#293). Needs both QHttpServer
# (for the SOAP route) and OpenSSL (for the self-signed cert
# generator) — gated on Qt6::HttpServer for the same reason as
# test_wsman_client.
if(TARGET Qt6::HttpServer AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_wsman_trust_retry.cpp)
    qumesh_wsman_test(test_wsman_trust_retry)
    target_link_libraries(test_wsman_trust_retry PRIVATE
        Qt6::HttpServer
        OpenSSL::Crypto)
endif()
