#include <arrow/api.h>
#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <arrow/io/file.h>
#include <arrow/ipc/api.h>
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include <arrow/record_batch.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/type_fwd.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <iostream>

arrow::Status GenInitialFile() {
	// Make a couple 8-bit integer arrays and a 16-bit integer array -- just like
	// basic Arrow example.
	arrow::Int8Builder int8builder;
	int8_t days_raw[5] = {1, 12, 17, 23, 28};
	ARROW_RETURN_NOT_OK(int8builder.AppendValues(days_raw, 5));
	std::shared_ptr<arrow::Array> days;
	ARROW_ASSIGN_OR_RAISE(days, int8builder.Finish());

	int8_t months_raw[5] = {1, 3, 5, 7, 1};
	ARROW_RETURN_NOT_OK(int8builder.AppendValues(months_raw, 5));
	std::shared_ptr<arrow::Array> months;
	ARROW_ASSIGN_OR_RAISE(months, int8builder.Finish());

	arrow::Int16Builder int16builder;
	int16_t years_raw[5] = {1990, 2000, 1995, 2000, 1995};
	ARROW_RETURN_NOT_OK(int16builder.AppendValues(years_raw, 5));
	std::shared_ptr<arrow::Array> years;
	ARROW_ASSIGN_OR_RAISE(years, int16builder.Finish());

	// Get a vector of our Arrays
	std::vector<std::shared_ptr<arrow::Array>> columns = {days, months, years};

	// Make a schema to initialize the Table with
	std::shared_ptr<arrow::Field> field_day, field_month, field_year;
	std::shared_ptr<arrow::Schema> schema;

	field_day = arrow::field("Day", arrow::int8());
	field_month = arrow::field("Month", arrow::int8());
	field_year = arrow::field("Year", arrow::int16());

	schema = arrow::schema({field_day, field_month, field_year});
	// With the schema and data, create a Table
	std::shared_ptr<arrow::Table> table;
	table = arrow::Table::Make(schema, columns);

	// Write out test files in IPC, CSV, and Parquet for the example to use.
	std::shared_ptr<arrow::io::FileOutputStream> outfile;
	ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open("test_in.arrow"));
	ARROW_ASSIGN_OR_RAISE(std::shared_ptr<arrow::ipc::RecordBatchWriter> ipc_writer,
	  					arrow::ipc::MakeFileWriter(outfile, schema));
	ARROW_RETURN_NOT_OK(ipc_writer->WriteTable(*table));
	ARROW_RETURN_NOT_OK(ipc_writer->Close());

	ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open("test_in.csv"));
	ARROW_ASSIGN_OR_RAISE(auto csv_writer,
	  					arrow::csv::MakeCSVWriter(outfile, table->schema()));
	ARROW_RETURN_NOT_OK(csv_writer->WriteTable(*table));
	ARROW_RETURN_NOT_OK(csv_writer->Close());

	ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open("test_in.parquet"));
	PARQUET_THROW_NOT_OK(
	    parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, 5));

	return arrow::Status::OK();
}

arrow::Status RunMain()
{
	ARROW_RETURN_NOT_OK(GenInitialFile());

	std::shared_ptr<arrow::io::ReadableFile> infile;
	ARROW_ASSIGN_OR_RAISE(infile, arrow::io::ReadableFile::Open("test_in.arrow", arrow::default_memory_pool()));
	ARROW_ASSIGN_OR_RAISE(auto ipc_reader, arrow::ipc::RecordBatchFileReader::Open(infile));

	std::shared_ptr<arrow::RecordBatch> rbatch;
	ARROW_ASSIGN_OR_RAISE(rbatch, ipc_reader->ReadRecordBatch(0));

	std::shared_ptr<arrow::io::FileOutputStream> outfile;
	ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open("test_out.arrow"));

	ARROW_ASSIGN_OR_RAISE(std::shared_ptr<arrow::ipc::RecordBatchWriter> ipc_writer, arrow::ipc::MakeFileWriter(outfile, rbatch->schema()));
	ARROW_RETURN_NOT_OK(ipc_writer->WriteRecordBatch(*rbatch));
	ARROW_RETURN_NOT_OK(ipc_writer->Close());
	return arrow::Status::OK();
}

int main()
{

	arrow::Status st = RunMain();
	if (not st.ok())
	{
		std::cerr << st << std::endl;
		return 1;
	}
	return 0;
}
