#include "dram_controller.h"

#define _DRAM_LATENCY		240	//Cycles

// initialized in main.cc
uint32_t DRAM_MTPS, DRAM_DBUS_RETURN_TIME,
         tRP, tRCD, tCAS;

void MEMORY_CONTROLLER::reset_remain_requests(PACKET_QUEUE *queue, uint32_t channel)
{
    assert(0);

    for (uint32_t i=0; i<queue->SIZE; i++) {
        if (queue->entry[i].scheduled) {

            uint64_t op_addr = queue->entry[i].address;
            uint32_t op_cpu = queue->entry[i].cpu,
                     op_channel = dram_get_channel(op_addr), 
                     op_rank = dram_get_rank(op_addr), 
                     op_bank = dram_get_bank(op_addr), 
                     op_row = dram_get_row(op_addr);

            // update open row
            if ((bank_request[op_channel][op_rank][op_bank].cycle_available - tCAS) <= current_core_cycle[op_cpu])
                bank_request[op_channel][op_rank][op_bank].open_row = op_row;
            else
                bank_request[op_channel][op_rank][op_bank].open_row = UINT32_MAX;

            // this bank is ready for another DRAM request
            bank_request[op_channel][op_rank][op_bank].request_index = -1;
            bank_request[op_channel][op_rank][op_bank].row_buffer_hit = 0;
            bank_request[op_channel][op_rank][op_bank].working = 0;
            bank_request[op_channel][op_rank][op_bank].cycle_available = current_core_cycle[op_cpu];
            if (bank_request[op_channel][op_rank][op_bank].is_write) {
                scheduled_writes[channel]--;
                bank_request[op_channel][op_rank][op_bank].is_write = 0;
            }
            else if (bank_request[op_channel][op_rank][op_bank].is_read) {
                scheduled_reads[channel]--;
                bank_request[op_channel][op_rank][op_bank].is_read = 0;
            }

            queue->entry[i].scheduled = 0;
            queue->entry[i].event_cycle = current_core_cycle[op_cpu];
        }
    }
    
    update_schedule_cycle(&RQ[channel]);
    update_schedule_cycle(&WQ[channel]);
    update_process_cycle(&RQ[channel]);
    update_process_cycle(&WQ[channel]);

#ifdef SANITY_CHECK
    if (queue->is_WQ) {
        if (scheduled_writes[channel] != 0)
            assert(0);
    }
    else {
        if (scheduled_reads[channel] != 0)
            assert(0);
    }
#endif
}

void MEMORY_CONTROLLER::operate()
{
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        if ((write_mode[i] == 0) && (WQ[i].occupancy >= DRAM_WRITE_HIGH_WM)) {
    		assert(0);

            write_mode[i] = 1;

            // reset scheduled RQ requests
            reset_remain_requests(&RQ[i], i);
            // add data bus turn-around time
            dbus_cycle_available[i] += DRAM_DBUS_TURN_AROUND_TIME;
        } else if (write_mode[i]) {

    		assert(0);

            if (WQ[i].occupancy == 0)
                write_mode[i] = 0;
            else if (RQ[i].occupancy && (WQ[i].occupancy < DRAM_WRITE_LOW_WM))
                write_mode[i] = 0;

            if (write_mode[i] == 0) {
                // reset scheduled WQ requests
                reset_remain_requests(&WQ[i], i);
                // add data bus turnaround time
                dbus_cycle_available[i] += DRAM_DBUS_TURN_AROUND_TIME;
            }
        }

        // handle write
        // schedule new entry
        if (write_mode[i] && (WQ[i].next_schedule_index < WQ[i].SIZE)) {

        	
    		assert(0);

            if (WQ[i].next_schedule_cycle <= current_core_cycle[WQ[i].entry[WQ[i].next_schedule_index].cpu])
                schedule(&WQ[i]);
        }

        // process DRAM requests
        if (write_mode[i] && (WQ[i].next_process_index < WQ[i].SIZE)) {

        	
    		assert(0);

            if (WQ[i].next_process_cycle <= current_core_cycle[WQ[i].entry[WQ[i].next_process_index].cpu])
                process(&WQ[i]);
        }

        // handle read
        // schedule new entry
        if ((write_mode[i] == 0) && (RQ[i].next_schedule_index < RQ[i].SIZE)) {
            if (RQ[i].next_schedule_cycle <= current_core_cycle[RQ[i].entry[RQ[i].next_schedule_index].cpu])
                schedule(&RQ[i]);
        }

        // process DRAM requests
        if ((write_mode[i] == 0) && (RQ[i].next_process_index < RQ[i].SIZE)) {
            if (RQ[i].next_process_cycle <= current_core_cycle[RQ[i].entry[RQ[i].next_process_index].cpu])
                process(&RQ[i]);
        }
    }
}

void MEMORY_CONTROLLER::schedule(PACKET_QUEUE *queue)
{
	uint64_t read_addr;
	
	if (queue->is_WQ)
	{
		
		assert(0);
	}

    // first, search for the oldest open row hit
    for (uint32_t i=0; i<queue->SIZE; i++) {

        // already scheduled
        if (queue->entry[i].scheduled) 
            continue;

        // empty entry
        read_addr = queue->entry[i].address;
        if (read_addr == 0) 
            continue;

        uint64_t op_addr = queue->entry[i].address;
        uint32_t op_cpu = queue->entry[i].cpu,
                 op_channel = dram_get_channel(op_addr);

		scheduled_reads[op_channel]++;

		queue->entry[i].scheduled = 1;
        queue->entry[i].event_cycle = current_core_cycle[op_cpu] + _DRAM_LATENCY;

        update_schedule_cycle(queue);
        update_process_cycle(queue);
    }
}

void MEMORY_CONTROLLER::process(PACKET_QUEUE *queue)
{
	if (queue->is_WQ)
	{
		
		assert(0);
	}

    uint32_t request_index = queue->next_process_index;

    // sanity check
    if (request_index == queue->SIZE)
        assert(0);

    uint64_t op_addr = queue->entry[request_index].address;
    uint32_t op_cpu = queue->entry[request_index].cpu,
             op_channel = dram_get_channel(op_addr);

    // send data back to the core cache hierarchy
    upper_level_dcache[op_cpu]->return_data(&queue->entry[request_index]);
    scheduled_reads[op_channel]--;

    // remove the oldest entry
    queue->remove_queue(&queue->entry[request_index]);
    update_process_cycle(queue);
}

int MEMORY_CONTROLLER::add_rq(PACKET *packet)
{
    // simply return read requests with dummy response before the warmup
    if (all_warmup_complete < NUM_CPUS) {
        if (packet->instruction) 
            upper_level_icache[packet->cpu]->return_data(packet);
        else // data
            upper_level_dcache[packet->cpu]->return_data(packet);

        return -1;
    }

    // check for the latest wirtebacks in the write queue
    uint32_t channel = dram_get_channel(packet->address);
    int wq_index = check_dram_queue(&WQ[channel], packet);

    if (wq_index != -1) {

    	
    	assert(0);

        packet->data = WQ[channel].entry[wq_index].data;
        if (packet->instruction) 
            upper_level_icache[packet->cpu]->return_data(packet);
        else // data
            upper_level_dcache[packet->cpu]->return_data(packet);

        ACCESS[1]++;
        HIT[1]++;

        WQ[channel].FORWARD++;
        RQ[channel].ACCESS++;

        return -1;
    }

    // check for duplicates in the read queue
    int index = check_dram_queue(&RQ[channel], packet);
    if (index != -1)
    {

        return index; // merged index
    }

    // search for the empty index
    for (index=0; index<DRAM_RQ_SIZE; index++) {
        if (RQ[channel].entry[index].address == 0) {
            
            memcpy(&RQ[channel].entry[index], packet, sizeof(PACKET));
            RQ[channel].occupancy++;

            break;
        }
    }

    update_schedule_cycle(&RQ[channel]);

    return -1;
}

int MEMORY_CONTROLLER::add_wq(PACKET *packet)
{
    return -1;	
}

int MEMORY_CONTROLLER::add_pq(PACKET *packet)
{
    
    assert(0);
    return -1;
}

void MEMORY_CONTROLLER::return_data(PACKET *packet)
{
	
    assert(0);
}

void MEMORY_CONTROLLER::update_schedule_cycle(PACKET_QUEUE *queue)
{
	if (queue->is_WQ)
	{
		
		assert(0);
	}

    // update next_schedule_cycle
    uint64_t min_cycle = UINT64_MAX;
    uint32_t min_index = queue->SIZE;
    for (uint32_t i=0; i<queue->SIZE; i++) {

        if (queue->entry[i].address && (queue->entry[i].scheduled == 0) && (queue->entry[i].event_cycle < min_cycle)) {
            min_cycle = queue->entry[i].event_cycle;
            min_index = i;
        }
    }
    
    queue->next_schedule_cycle = min_cycle;
    queue->next_schedule_index = min_index;
}

void MEMORY_CONTROLLER::update_process_cycle(PACKET_QUEUE *queue)
{
    // update next_process_cycle
    uint64_t min_cycle = UINT64_MAX;
    uint32_t min_index = queue->SIZE;
    for (uint32_t i=0; i<queue->SIZE; i++) {
        if (queue->entry[i].scheduled && (queue->entry[i].event_cycle < min_cycle)) {
            min_cycle = queue->entry[i].event_cycle;
            min_index = i;
        }
    }
    
    queue->next_process_cycle = min_cycle;
    queue->next_process_index = min_index;
}

int MEMORY_CONTROLLER::check_dram_queue(PACKET_QUEUE *queue, PACKET *packet)
{
    // search write queue
    for (uint32_t index=0; index<queue->SIZE; index++) {
        if (queue->entry[index].address == packet->address) {

        	if (queue->is_WQ)
        	{
        		
        		assert(0);
        	}

            return index;
        }
    }

    return -1;
}

uint32_t MEMORY_CONTROLLER::dram_get_channel(uint64_t address)
{
    if (LOG2_DRAM_CHANNELS == 0)
        return 0;

    int shift = 0;

    return (uint32_t) (address >> shift) & (DRAM_CHANNELS - 1);
}

uint32_t MEMORY_CONTROLLER::dram_get_bank(uint64_t address)
{
    if (LOG2_DRAM_BANKS == 0)
        return 0;

    int shift = LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_BANKS - 1);
}

uint32_t MEMORY_CONTROLLER::dram_get_column(uint64_t address)
{
    if (LOG2_DRAM_COLUMNS == 0)
        return 0;

    int shift = LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_COLUMNS - 1);
}

uint32_t MEMORY_CONTROLLER::dram_get_rank(uint64_t address)
{
    if (LOG2_DRAM_RANKS == 0)
        return 0;

    int shift = LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_RANKS - 1);
}

uint32_t MEMORY_CONTROLLER::dram_get_row(uint64_t address)
{
    if (LOG2_DRAM_ROWS == 0)
        return 0;

    int shift = LOG2_DRAM_RANKS + LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_ROWS - 1);
}

uint32_t MEMORY_CONTROLLER::get_occupancy(uint8_t queue_type, uint64_t address)
{
    uint32_t channel = dram_get_channel(address);
    if (queue_type == 1)
        return RQ[channel].occupancy;
    else if (queue_type == 2)
        return WQ[channel].occupancy;

    return 0;
}

uint32_t MEMORY_CONTROLLER::get_size(uint8_t queue_type, uint64_t address)
{
    uint32_t channel = dram_get_channel(address);
    if (queue_type == 1)
        return RQ[channel].SIZE;
    else if (queue_type == 2)
        return WQ[channel].SIZE;

    return 0;
}

void MEMORY_CONTROLLER::increment_WQ_FULL(uint64_t address)
{
    uint32_t channel = dram_get_channel(address);
    WQ[channel].FULL++;
}
