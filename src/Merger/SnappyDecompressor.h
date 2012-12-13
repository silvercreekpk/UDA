/*
 * SnappyDecompressor.h
 *
 *  Created on: Nov 22, 2012
 *      Author: dinal
 */

#include "UdaBridge.h"
#include <string>
#include "../DataNet/RDMAClient.h"
#include <dlfcn.h>
#include "../DataNet/RDMAComm.h"
#include "snappy-c.h"


#ifndef SNAPPYDECOMPRESSOR_H_
#define SNAPPYDECOMPRESSOR_H_

class SnappyDecompressor : public DecompressorWrapper
{
	public:
		SnappyDecompressor(int port, reduce_task_t* reduce_task);
		void initDecompress();
		void decompress (char* compressed, int length);
		decompressRetData_t* get_next_block_length(char* buf);
		int getBlockSizeOffset ();
		decompressRetData_t* decompress(char* compressed_buff, char* uncompressed_buff, size_t compressed_buff_len, size_t uncompressed_buff_len,int offest);


	private:
		void *libsnappy;
		int snappy_loaded ;
		//void *decompressor_func_ptr;

		void init();
		void loadDecompressorFunc();
		//void* loadSymbol(void *handle, char *symbol );
};

#endif /* SNAPPYDECOMPRESSOR_H_ */
