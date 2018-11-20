
set(THREAD_LIB_SRC threadpool.cpp threadmutex.hpp)
add_library(thread ${THREAD_LIB_SRC})



add_executable(Thread_Test test.cpp threadpool.cpp)
target_link_libraries(Thread_Test ${EXTRA_LIBS})