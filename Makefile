#Makefile

GROVE_DIR = .

SRC_DIR = $(GROVE_DIR)/src
TOOL_DIR = $(GROVE_DIR)/tools
TEST_DIR = $(GROVE_DIR)/test
TP_DIR = $(GROVE_DIR)/third_party


OBJ_DIR = $(GROVE_DIR)/obj
LIB_DIR = $(GROVE_DIR)/lib
BIN_DIR = $(GROVE_DIR)/bin


HEADER_PATH = /usr/local/include/grove
LIB_PATH = /usr/local/lib
CONF_PATH = /etc/grove/
DATA_PATH = /var/lib/grove

CXX = g++

CXXFLAGS = -std=c++2a -g -Wall -Wextra -pthread -w -fno-rtti

CPPFLAGS = -isystem $(GROVE_DIR)/include -isystem $(GTEST_DIR)/include -isystem $(TP_DIR)

LDFLAGS = -L/usr/local/lib

LDLIBS = -lpthread -lrocksdb -ldl -lsnappy -lbz2 -lrt -lz -llz4 -lzstd -lyaml-cpp -lroaring -ldb_cxx-5.3 -lwiredtiger

# LDLIBS = -lpthread -lrocksdb -lorc -lsnappy -lbz2 -lrt -lz -llz4 -lzstd -lyaml-cpp -lserd-0 \
         -lprotobuf -lprotoc -lhdfspp_static -lsasl2 -lcrypto -lroaring -ldb_cxx-5.3 -lwiredtiger


MPICXX = mpicxx


BMP_OBJS = $(OBJ_DIR)/bitvector.o $(OBJ_DIR)/roaring_bitvector.o
BUF_OBJS = $(OBJ_DIR)/buffer_manager.o $(OBJ_DIR)/buffer_page.o
DB_OBJS = $(OBJ_DIR)/config.o $(OBJ_DIR)/database.o $(OBJ_DIR)/database_builder.o \
          $(OBJ_DIR)/statistics_manager.o
OPR_OBJS = $(OBJ_DIR)/operator.o \
           $(OBJ_DIR)/table_scan.o $(OBJ_DIR)/triple_index_lookup.o \
		   $(OBJ_DIR)/switch.o $(OBJ_DIR)/filter.o \
           $(OBJ_DIR)/hash_join.o $(OBJ_DIR)/backprobe_hash_join.o \
		   $(OBJ_DIR)/leapfrog_trie_join.o $(OBJ_DIR)/results_printer.o
PARSER_OBJS = $(OBJ_DIR)/rdf_util.o $(OBJ_DIR)/sparql_lexer.o $(OBJ_DIR)/sparql_parser.o \
              $(OBJ_DIR)/turtle_lexer.o $(OBJ_DIR)/turtle_parser.o
PLAN_OBJS = $(OBJ_DIR)/query_plan.o $(OBJ_DIR)/query_planner.o $(OBJ_DIR)/query_decomposer.o
QUERY_OBJS = $(OBJ_DIR)/query_graph.o $(OBJ_DIR)/semantic_analyzer.o
RTM_OBJS = $(OBJ_DIR)/code_generator.o $(OBJ_DIR)/runtime.o
STG_OBJS = $(OBJ_DIR)/dictionary.o $(OBJ_DIR)/triple_table.o \
           $(OBJ_DIR)/triple_index.o $(OBJ_DIR)/aggregated_index.o \
		   $(OBJ_DIR)/bdb_file.o $(OBJ_DIR)/wired_tiger.o
THRD_OBJS = $(OBJ_DIR)/condition.o $(OBJ_DIR)/mutex.o $(OBJ_DIR)/rw_latch.o $(OBJ_DIR)/thread.o
UTIL_OBJS = $(OBJ_DIR)/bit_set.o $(OBJ_DIR)/byte_buffer.o \
            $(OBJ_DIR)/file_directory.o $(OBJ_DIR)/math_functions.o \
			$(OBJ_DIR)/string_util.o

ALL_OBJS = $(BMP_OBJS) $(BUF_OBJS) $(DB_OBJS) $(OPR_OBJS) $(PARSER_OBJS) $(PLAN_OBJS) \
           $(QUERY_OBJS) $(RTM_OBJS) $(STG_OBJS) $(THRD_OBJS) $(UTIL_OBJS)

TEST_OBJS = $(OBJ_DIR)/test_main.o $(OBJ_DIR)/bitvector_test.o \
						$(OBJ_DIR)/hash_table_test.o $(OBJ_DIR)/memory_pool_test.o $(OBJ_DIR)/static_vector_test.o \
            $(OBJ_DIR)/sparql_parser_test.o $(OBJ_DIR)/turtle_parser_test.o


TP_OBJS = $(OBJ_DIR)/murmur_hash3.o


TARGET = $(BIN_DIR)/grovetool


all: build


build: dirs $(TARGET)
	@ echo "Build finished."


test: dirs $(LIB_DIR)/libgrove.a $(BIN_DIR)/grovetest
	@ echo "Tests starting..."
	@ $(BIN_DIR)/grovetest


install: build
	@ mkdir -p $(HEADER_PATH) $(CONF_PATH) $(DATA_PATH)
	@ chmod 777 $(DATA_PATH)
	@ # cp $(GROVE_DIR)/include/*.h $(HEADER_PATH)
	@ echo "Installation finished."


clean:
	@ rm -f $(OBJ_DIR)/*.o $(LIB_DIR)/*.a $(BIN_DIR)/*


dirs:
	@ mkdir -p $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)



#Grove Tools
$(BIN_DIR)/grovetool: $(OBJ_DIR)/command.o $(OBJ_DIR)/grove_tool.o $(LIB_DIR)/libgrove.a
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BIN_DIR)/groveload_mpi: $(LIB_DIR)/libgrove.a
	#$(MPICXX) $(CPPFLAGS) -o $@ $^

$(OBJ_DIR)/command.o: $(TOOL_DIR)/command.h $(TOOL_DIR)/command.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TOOL_DIR)/command.cpp

$(OBJ_DIR)/grove_tool.o: $(TOOL_DIR)/grove_tool.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TOOL_DIR)/grove_tool.cpp

#Grove Library
$(LIB_DIR)/libgrove.a: $(ALL_OBJS) $(TP_OBJS)
	$(AR) $(ARFLAGS) $@ $^



$(OBJ_DIR)/bitvector.o: $(SRC_DIR)/bitmap/bitvector.h $(SRC_DIR)/bitmap/bitvector.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/bitmap/bitvector.cpp

$(OBJ_DIR)/roaring_bitvector.o: $(SRC_DIR)/bitmap/roaring_bitvector.h $(SRC_DIR)/bitmap/roaring_bitvector.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/bitmap/roaring_bitvector.cpp

$(OBJ_DIR)/config.o: $(SRC_DIR)/database/config.h $(SRC_DIR)/database/config.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/database/config.cpp

$(OBJ_DIR)/database.o: $(SRC_DIR)/database/database.h $(SRC_DIR)/database/database.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/database/database.cpp

$(OBJ_DIR)/database_builder.o: $(SRC_DIR)/database/database_builder.h $(SRC_DIR)/database/database_builder.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/database/database_builder.cpp

$(OBJ_DIR)/statistics_manager.o: $(SRC_DIR)/database/statistics_manager.h $(SRC_DIR)/database/statistics_manager.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/database/statistics_manager.cpp

$(OBJ_DIR)/operator.o: $(SRC_DIR)/operator/operator.h $(SRC_DIR)/operator/operator.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/operator.cpp

$(OBJ_DIR)/table_scan.o: $(SRC_DIR)/operator/table_scan.h $(SRC_DIR)/operator/table_scan.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/table_scan.cpp

$(OBJ_DIR)/triple_index_lookup.o: $(SRC_DIR)/operator/triple_index_lookup.h $(SRC_DIR)/operator/triple_index_lookup.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/triple_index_lookup.cpp

$(OBJ_DIR)/switch.o: $(SRC_DIR)/operator/switch.h $(SRC_DIR)/operator/switch.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/switch.cpp

$(OBJ_DIR)/filter.o: $(SRC_DIR)/operator/filter.h $(SRC_DIR)/operator/filter.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/filter.cpp

$(OBJ_DIR)/hash_join.o: $(SRC_DIR)/operator/hash_join.h $(SRC_DIR)/operator/hash_join.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/hash_join.cpp

$(OBJ_DIR)/backprobe_hash_join.o: $(SRC_DIR)/operator/backprobe_hash_join.h $(SRC_DIR)/operator/backprobe_hash_join.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/backprobe_hash_join.cpp

$(OBJ_DIR)/leapfrog_trie_join.o: $(SRC_DIR)/operator/leapfrog_trie_join.h $(SRC_DIR)/operator/leapfrog_trie_join.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/leapfrog_trie_join.cpp

$(OBJ_DIR)/results_printer.o: $(SRC_DIR)/operator/results_printer.h $(SRC_DIR)/operator/results_printer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/operator/results_printer.cpp

$(OBJ_DIR)/rdf_util.o: $(SRC_DIR)/parser/rdf_util.h $(SRC_DIR)/parser/rdf_util.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/parser/rdf_util.cpp

$(OBJ_DIR)/sparql_lexer.o: $(SRC_DIR)/parser/sparql_lexer.h $(SRC_DIR)/parser/sparql_lexer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/parser/sparql_lexer.cpp

$(OBJ_DIR)/sparql_parser.o: $(SRC_DIR)/parser/sparql_parser.h $(SRC_DIR)/parser/sparql_parser.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/parser/sparql_parser.cpp

$(OBJ_DIR)/turtle_lexer.o: $(SRC_DIR)/parser/turtle_lexer.h $(SRC_DIR)/parser/turtle_lexer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/parser/turtle_lexer.cpp

$(OBJ_DIR)/turtle_parser.o: $(SRC_DIR)/parser/turtle_parser.h $(SRC_DIR)/parser/turtle_parser.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/parser/turtle_parser.cpp

$(OBJ_DIR)/query_plan.o: $(SRC_DIR)/plan/query_plan.h $(SRC_DIR)/plan/query_plan.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/plan/query_plan.cpp

$(OBJ_DIR)/query_planner.o: $(SRC_DIR)/plan/query_planner.h $(SRC_DIR)/plan/query_planner.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/plan/query_planner.cpp

$(OBJ_DIR)/query_decomposer.o: $(SRC_DIR)/plan/query_decomposer.h $(SRC_DIR)/plan/query_decomposer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/plan/query_decomposer.cpp

$(OBJ_DIR)/query_graph.o: $(SRC_DIR)/query/query_graph.h $(SRC_DIR)/query/query_graph.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/query/query_graph.cpp

$(OBJ_DIR)/semantic_analyzer.o: $(SRC_DIR)/query/semantic_analyzer.h $(SRC_DIR)/query/semantic_analyzer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/query/semantic_analyzer.cpp

$(OBJ_DIR)/code_generator.o: $(SRC_DIR)/runtime/code_generator.h $(SRC_DIR)/runtime/code_generator.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/runtime/code_generator.cpp

$(OBJ_DIR)/runtime.o: $(SRC_DIR)/runtime/runtime.h $(SRC_DIR)/runtime/runtime.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/runtime/runtime.cpp

$(OBJ_DIR)/buffer_manager.o: $(SRC_DIR)/buffer/buffer_manager.h $(SRC_DIR)/buffer/buffer_manager.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/buffer/buffer_manager.cpp

$(OBJ_DIR)/buffer_page.o: $(SRC_DIR)/buffer/buffer_page.h $(SRC_DIR)/buffer/buffer_page.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/buffer/buffer_page.cpp

$(OBJ_DIR)/dictionary.o: $(SRC_DIR)/storage/dictionary.h $(SRC_DIR)/storage/dictionary.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/storage/dictionary.cpp

$(OBJ_DIR)/triple_table.o: $(SRC_DIR)/storage/triple_table.h $(SRC_DIR)/storage/triple_table.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/storage/triple_table.cpp

$(OBJ_DIR)/triple_index.o: $(SRC_DIR)/storage/triple_index.h $(SRC_DIR)/storage/triple_index.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/storage/triple_index.cpp

$(OBJ_DIR)/aggregated_index.o: $(SRC_DIR)/storage/aggregated_index.h $(SRC_DIR)/storage/aggregated_index.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/storage/aggregated_index.cpp

$(OBJ_DIR)/bdb_file.o: $(SRC_DIR)/storage/bdb_file.h $(SRC_DIR)/storage/bdb_file.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/storage/bdb_file.cpp

$(OBJ_DIR)/wired_tiger.o: $(SRC_DIR)/storage/wired_tiger.h $(SRC_DIR)/storage/wired_tiger.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/storage/wired_tiger.cpp

$(OBJ_DIR)/condition.o: $(SRC_DIR)/thread/condition.h $(SRC_DIR)/thread/condition.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/thread/condition.cpp

$(OBJ_DIR)/mutex.o: $(SRC_DIR)/thread/mutex.h $(SRC_DIR)/thread/mutex.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/thread/mutex.cpp

$(OBJ_DIR)/rw_latch.o: $(SRC_DIR)/thread/rw_latch.h $(SRC_DIR)/thread/rw_latch.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/thread/rw_latch.cpp

$(OBJ_DIR)/thread.o: $(SRC_DIR)/thread/thread.h $(SRC_DIR)/thread/thread.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/thread/thread.cpp

$(OBJ_DIR)/bit_set.o: $(SRC_DIR)/util/bit_set.h $(SRC_DIR)/util/bit_set.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/util/bit_set.cpp

$(OBJ_DIR)/byte_buffer.o: $(SRC_DIR)/util/byte_buffer.h $(SRC_DIR)/util/byte_buffer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/util/byte_buffer.cpp

$(OBJ_DIR)/file_reader_writer.o: $(SRC_DIR)/util/file_reader_writer.h $(SRC_DIR)/util/file_reader_writer.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/util/file_reader_writer.cpp

$(OBJ_DIR)/file_directory.o: $(SRC_DIR)/util/file_directory.h $(SRC_DIR)/util/file_directory.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/util/file_directory.cpp

$(OBJ_DIR)/math_functions.o: $(SRC_DIR)/util/math_functions.h $(SRC_DIR)/util/math_functions.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/util/math_functions.cpp

$(OBJ_DIR)/string_util.o: $(SRC_DIR)/util/string_util.h $(SRC_DIR)/util/string_util.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(SRC_DIR)/util/string_util.cpp



#Grove Test
$(BIN_DIR)/grovetest: $(OBJ_DIR)/gtest-all.o $(TEST_OBJS) $(LIB_DIR)/libgrove.a
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)


$(OBJ_DIR)/test_main.o: $(TEST_DIR)/test_main.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/test_main.cpp

$(OBJ_DIR)/bitvector_test.o: $(TEST_DIR)/bitmap/bitvector_test.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/bitmap/bitvector_test.cpp

$(OBJ_DIR)/sparql_parser_test.o: $(TEST_DIR)/parser/sparql_parser_test.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/parser/sparql_parser_test.cpp

$(OBJ_DIR)/turtle_parser_test.o: $(TEST_DIR)/parser/turtle_parser_test.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/parser/turtle_parser_test.cpp

$(OBJ_DIR)/hash_table_test.o: $(TEST_DIR)/util/hash_table_test.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/util/hash_table_test.cpp

$(OBJ_DIR)/memory_pool_test.o: $(TEST_DIR)/util/memory_pool_test.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/util/memory_pool_test.cpp

$(OBJ_DIR)/static_vector_test.o: $(TEST_DIR)/util/static_vector_test.cpp
	$(CXX) $(CPPFLAGS) -I$(SRC_DIR) $(CXXFLAGS) -c -o $@ $(TEST_DIR)/util/static_vector_test.cpp



#Third Party

#Smhasher
$(OBJ_DIR)/murmur_hash3.o: $(TP_DIR)/smhasher/MurmurHash3.h $(TP_DIR)/smhasher/MurmurHash3.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(TP_DIR)/smhasher/MurmurHash3.cpp


#Gtest
GTEST_DIR = $(GROVE_DIR)/third_party/googletest

GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

GTEST_SRCS = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

$(OBJ_DIR)/gtest-all.o: $(GTEST_SRCS)
	$(CXX) -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) $(CXXFLAGS) -c -o $@ $(GTEST_DIR)/src/gtest-all.cc
