include_directories(BEFORE ${SOURCE_FILES_DIR}/src)

source_group("Source"   FILES ./redis_reply_test.c)
add_executable(redis_reply_test ./redis_reply_test.c)
target_link_libraries(redis_reply_test RedisProto)

source_group("Source"   FILES ./redis_request_test.c)
add_executable(redis_request_test ./redis_request_test.c)
target_link_libraries(redis_request_test RedisProto)