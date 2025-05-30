#pragma once
#include "common/type_definitions.h"
#include <memory>
#include <mutex>

class ReadPageGuard
{
	private:
	page_id_t page_id;	
		
};

class WritePageGuard
{
	private:
	page_id_t page_id;	
};
