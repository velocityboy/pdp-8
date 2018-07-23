project(src)

add_library(pdp8
  pdp8/src/dislib.c
  pdp8/src/pdp8_console.c
  pdp8/src/pdp8_cpu.c
  pdp8/src/pdp8_eae.c
  pdp8/src/pdp8_rk8e.c
  pdp8/src/pdp8_mempar.c
  pdp8/src/pdp8_mex_tso.c
  pdp8/src/pdp8_punch.c
  pdp8/src/pdp8_trace.c
  pdp8/src/pdp8_util.c
  pdp8/src/logger.c
  pdp8/src/scheduler.c)

target_link_libraries(pdp8 buffer)

add_executable(pdp8_tests
  pdp8/tests/testmain.c
  pdp8/tests/test_console.c
  pdp8/tests/test_cpu.c
  pdp8/tests/test_cpu_group1.c
  pdp8/tests/test_cpu_group2.c
  pdp8/tests/test_eae.c
  pdp8/tests/test_interrupts.c
  pdp8/tests/test_mex.c
  pdp8/tests/test_punch.c
  pdp8/tests/test_scheduler.c)
target_link_libraries(pdp8_tests pdp8)

target_include_directories(pdp8
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE src)

add_executable(disasm disasm/src/disasm.c)
target_link_libraries(disasm pdp8)

add_executable(bootprom bootprom/src/bootprom.c)
target_link_libraries(bootprom pdp8)

add_library(buffer
  buffer/src/ring_buffer.c
  buffer/src/lin_buffer.c)

target_include_directories(buffer
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE src)

add_executable(buffer_tests
  pdp8/tests/testmain.c
  buffer/tests/test_ring_buffer.c
)
target_link_libraries(buffer_tests buffer)

add_executable(emu
  emu/src/bootprom.c
  emu/src/emu.c
  emu/src/commandset.c
  emu/src/options.c
  emu/src/pt_driver.c
  emu/src/rk_driver.c
  emu/src/tty_driver.c)
target_link_libraries(emu pdp8)

add_executable(ptrim ptrim/src/ptrim.c)
target_link_libraries(ptrim pdp8)
