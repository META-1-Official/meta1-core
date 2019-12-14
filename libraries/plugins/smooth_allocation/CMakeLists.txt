file(GLOB HEADERS "include/graphene/smooth_allocation/*.hpp")

add_library( graphene_smooth_allocation
             smooth_allocation_plugin.cpp
           )

target_link_libraries( graphene_smooth_allocation graphene_chain graphene_app )
target_include_directories( graphene_smooth_allocation
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( smooth_allocation_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_smooth_allocation

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/smooth_allocation" )