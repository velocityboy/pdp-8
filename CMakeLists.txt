cmake_minimum_required(VERSION 2.4)

project(PDP8)
add_executable(disasm
  disasm/disasm.c)

target_link_libraries(disasm dislib)

add_executable(emu
  emu/emu.c)

target_link_libraries(emu dislib)

add_library(dislib STATIC
  disasm/dislib.c
)

add_executable(dumppair bootprom/dumppair.c)
target_link_libraries(dumppair dislib)

add_executable(ptrim ptrim/ptrim.c)
target_link_libraries(ptrim dislib)
