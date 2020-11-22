# mock this until we can actually detect it
#set(SDDC_LIBRARIES "-lsddc")

add_executable(sddc_connector sddc_connector.cpp sdr_cuda.cu)
target_link_libraries(sddc_connector ${OWRX_CONNECTOR_LIBRARIES} ${SDDC_LIBRARIES} ${LIBCSDR_LIBRARIES})
target_include_directories(sddc_connector PRIVATE ${OWRX_CONNECTOR_INCLUDE_DIRS})

install(TARGETS sddc_connector DESTINATION ${CMAKE_INSTALL_BINDIR})
