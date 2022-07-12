#include <stdio.h>

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include "../../include/FS.h"
#include "../../include/settings.h"
#include "../../include/types.h"
#include "../../bench/bench.h"
#include "../interface.h"
#include "../vectored_interface.h"

//#define TEST
static uint32_t write_length;
static uint32_t write_test_cnt;
static uint32_t read_length;
static uint32_t read_cnt;
#ifdef WRITE_STOP_READ
fdriver_lock_t write_check_lock;
volatile uint32_t write_cnt;
#endif
void log_print(int sig){
    printf("write avg length:%.2f (length:cnt %u:%u)\n", (double)write_length/write_test_cnt, write_length, write_test_cnt);
    printf("read avg length:%.2f (length:cnt %u:%u)\n\n", (double)read_length/read_cnt, read_length, read_cnt);
	inf_free();
	printf("f inf\n");
	fflush(stdout);
	fflush(stderr);
	sync();
	printf("before exit\n");


	exit(1);
}

void print_temp_log(int sig){
    write_length=write_test_cnt=read_length=read_cnt=0;
	inf_print_log();
}

bool file_end_req(request *const req);


vec_request *make_vec_req(bool write_type, uint32_t start_offset, uint32_t length){
    static uint32_t global_seq=0;
    static uint32_t global_vac_seq=0;
	vec_request *res=(vec_request *)calloc(1, sizeof(vec_request));
    res->tag_id=0;
    res->seq_id=global_vac_seq++;
    res->size=length;
    res->type=(write_type?FS_SET_T:FS_GET_T);
    res->req_array=(request*)calloc(res->size, sizeof(request));
    uint32_t prev_lba=UINT32_MAX;
    uint32_t consecutive_cnt=0;

    if(write_type){
        write_test_cnt++;
        write_length+=res->size;
    }
    else{
        read_cnt++;
        read_length+=res->size;
    }
    for(int i=0; i<res->size; i++){
        request *temp=&res->req_array[i];
        temp->parents=res;
        temp->type=res->type;
        temp->seq=i;
        temp->end_req=file_end_req;
        temp->type_ftl=temp->type_lower=0;	
        temp->is_sequential_start=false;
		temp->flush_all=0;
		temp->global_seq=global_seq++;
        temp->value=inf_get_valueset(NULL, temp->type==FS_GET_T?FS_MALLOC_R:FS_MALLOC_W, PAGESIZE);

        temp->key=(start_offset+i)%RANGE;

		if(prev_lba==UINT32_MAX){
			prev_lba=temp->key;
		}
		else{
			if(prev_lba+1==temp->key){
				consecutive_cnt++;
			}
			else{
				res->req_array[i-consecutive_cnt-1].is_sequential_start=(consecutive_cnt!=0);
				res->req_array[i-consecutive_cnt-1].consecutive_length=consecutive_cnt;
				consecutive_cnt=0;
			}
			prev_lba=temp->key;
			temp->consecutive_length=0;
		}
    }

	res->req_array[(res->size-1)-consecutive_cnt].is_sequential_start=(consecutive_cnt!=0);
	res->req_array[(res->size-1)-consecutive_cnt].consecutive_length=0;//consecutive_cnt;
    return res;
}

vec_request **translate_vec_request_array(bool write_type,long true_start_offset, uint32_t io_size){
    long true_end_offset=true_start_offset+io_size;
    true_start_offset/=LPAGESIZE;
    true_end_offset/=LPAGESIZE;
    uint32_t length=true_end_offset-true_start_offset+1;

    uint32_t vec_num=CEIL(length,QSIZE);
    vec_request **res=(vec_request**)calloc(vec_num+1,sizeof(vec_request));
    for(uint32_t i=0; i<vec_num; i++){
        uint32_t start_offset=true_start_offset+i*QSIZE;
        uint32_t end_offset=(true_start_offset+(i+1)*QSIZE-1> true_end_offset ? true_end_offset: true_start_offset+(i+1)*QSIZE-1);
        res[i]=make_vec_req(write_type, start_offset, end_offset-start_offset+1);
    }
    res[vec_num]=NULL;
    return res;
}

bool cutting_header(FILE *fp){
    char *line=NULL;
    size_t len=0;
    ssize_t read=getline(&line, &len, fp);
    if(read==-1) return false;
    while(strncmp(line,"EndHeader", strlen("EndHeader"))!=0){
        read=getline(&line, &len, fp);
    }
    free(line);
    return true;
}


vec_request **file_get_vec_request (FILE *fp){
    /*
    static bool isfirst=true;
    if(isfirst==true){
        cutting_header(fp);
        isfirst=false;
    }
    */
    char *line=NULL;
    size_t len=0;
    bool write_type;
retry:
    ssize_t read=getline(&line, &len, fp);
    if(read==-1) return NULL;
    char *ptr;
    ptr=strtok(line, ","); //get type
    if(ptr[0]=='B' || ptr[0]=='E'){
        goto retry;
    }
    else if(ptr[19]=='R'){
        write_type=false;
    }
    else if(ptr[18]=='W'){
        write_type=true;
    }
    else{
        goto retry;
    }

    ptr=strtok(NULL, ",");//ignore timestamp
    ptr=strtok(NULL, ",");//ignore Process
    ptr=strtok(NULL, ",");//ignore Thread ID
    ptr=strtok(NULL, ",");//ignore IrpPtr

    ptr=strtok(NULL, ",");//get byteoffset
    long true_start_offset=(uint64_t)strtol(ptr, NULL, 0);

    ptr=strtok(NULL, ",");//get length
    uint32_t io_size=(uint32_t)strtol(ptr, NULL, 0);
 
    vec_request **res=translate_vec_request_array(write_type, true_start_offset, io_size);

    if(line){
        free(line);
    }

    return res;   
}

vec_request **file_get_vec_request2 (FILE *fp){
    char *line=NULL;
    size_t len=0;
    ssize_t read=getline(&line, &len, fp);
    if(read==-1) return NULL;

    char *ptr;
    ptr=strtok(line, ",");
    for(uint32_t i=0; i<3; i++){ //skip
        ptr=strtok(NULL, ",");
    }

    bool write_type=ptr[0]=='W';

    ptr=strtok(NULL, ",");
    long true_start_offset=atol(ptr);
    ptr=strtok(NULL, ",");
    uint32_t io_size=atoi(ptr);

    vec_request **res=translate_vec_request_array(write_type, true_start_offset, io_size);

    if(line){
        free(line);
    }

    return res;
}

char *get_file_name(int *argc, char **argv){
	struct option options[]={
		{"trace",1,0,0},
		{0,0,0,0}
	};

	char *temp_argv[10];
	int temp_cnt=0;
	for(int i=0; i<*argc; i++){
		if(strncmp(argv[i],"--trace",strlen("--trace"))==0) continue;
		temp_argv[temp_cnt++]=argv[i];
	}
	temp_argv[temp_cnt]=NULL;
	if(temp_cnt==*argc) return NULL;
    int opt;
    int index;
    opterr=0;
    char *res=NULL;
    while((opt=getopt_long(*argc, argv, "", options, &index))!=-1){
        switch(opt){
            case 0:
                if(optarg!=NULL){
                    res=optarg;      
                }
            break;
            default:
            break;
        }
    }

	for(int i=0; i<=temp_cnt; i++){
		argv[i]=temp_argv[i];
	}
	*argc=temp_cnt;
	optind=0;
    return res;
}

void * thread_test(void *arg){
	vec_request **req=NULL;
    FILE *fp=(FILE*)arg;

    static uint32_t vec_req_cnt=0;
    while((req=file_get_vec_request(fp))){
        for(int i=0; ; i++){
            if(req[i]==NULL){
                free(req);
                break;
            }
#ifdef WRITE_STOP_READ
            if (req[i]->type == FS_SET_T)
            {
                fdriver_lock(&write_check_lock);
                write_cnt++;
                fdriver_unlock(&write_check_lock);
            }
            else if (req[i]->type == FS_GET_T)
            {
                while (write_cnt != 0)
                {
                }
            }
#endif
            if (++vec_req_cnt % 10000 == 0)
            {
                printf("%u\n", vec_req_cnt);
            }
            assign_vectored_req(req[i]);
        }
    }

	return NULL;
}

pthread_t thr; 
int main(int argc,char* argv[]){
#ifdef WRITE_STOP_READ
    fdriver_mutex_init(&write_check_lock);
#endif
	struct sigaction sa;
	sa.sa_handler = log_print;
	sigaction(SIGINT, &sa, NULL);

	sigset_t tSigSetMask;
	sigdelset(&tSigSetMask, SIGINT);
	pthread_sigmask(SIG_SETMASK, &tSigSetMask, NULL);

	struct sigaction sa2={0};
	sa2.sa_handler = print_temp_log;
	sigaction(SIGCONT, &sa2, NULL);

	printf("signal add!\n");
    char *file_path=get_file_name(&argc, argv);
    FILE *fp;
    if(file_path==NULL){
        printf("please insert trace file name\n");
        exit(1);
    }
    else{
        fp=fopen(file_path, "r");
        if(fp==NULL){
            printf("please check file name!!\n");
            exit(1);
        }
    }
#ifdef TEST
    inf_init(1,0, argc, argv);
#else
	inf_init(0,0, argc, argv);
    
	bench_init();
	bench_vectored_configure();
	bench_add(VECTOREDUNIQRSET, 0, RANGE, RANGE);

	char *value;
	uint32_t mark;
	while((value=get_vectored_bench(&mark))){
		inf_vector_make_req(value, bench_transaction_end_req, mark);
	}

	while(!bench_is_finish()){
#ifdef LEAKCHECK
		sleep(1);
#endif
	}

	print_temp_log(0);

	bench_add(NOR, 0, -1, 0);
#endif
	pthread_create(&thr, NULL, thread_test, (void*)fp);
	pthread_join(thr, NULL);

    printf("write avg length:%.2f (length:cnt %u:%u)\n", (double)write_length / write_test_cnt, write_length, write_test_cnt);
    printf("read avg length:%.2f (length:cnt %u:%u)\n\n", (double)read_length / read_cnt, read_length, read_cnt);

    fclose(fp);
	inf_free();
	return 0;
}

extern pthread_mutex_t req_cnt_lock;
extern master_processor mp;
bool file_end_req(request *const req){
	vectored_request *preq=req->parents;
	//printf("req end_req:%u\n", req->key);
	uint32_t temp_crc;
#ifdef TEST
    req->mark=0;
#else
    req->mark=1;
#endif
    switch (req->type){
    case FS_NOTFOUND_T:
        bench_reap_data(req, mp.li);
        inf_free_valueset(req->value, FS_MALLOC_R);
        break;
    case FS_GET_T:
        bench_reap_data(req, mp.li);
        inf_free_valueset(req->value, FS_MALLOC_R);
        break;
    case FS_SET_T:
        bench_reap_data(req, mp.li);
        if(req->value){
           inf_free_valueset(req->value, FS_MALLOC_W);
        }
        break;
	case FS_FLUSH_T:
	case FS_DELETE_T:
		break;
    }

    pthread_mutex_lock(&req_cnt_lock);
	preq->done_cnt++;

	release_each_req(req);
	if(preq->size==preq->done_cnt){
#ifdef WRITE_STOP_READ
		if(preq->type==FS_SET_T){
			fdriver_lock(&write_check_lock);
			write_cnt--;
			fdriver_unlock(&write_check_lock);
		}
#endif
		free(preq->req_array);
		free(preq);
	}
	pthread_mutex_unlock(&req_cnt_lock);
    return true;
}
