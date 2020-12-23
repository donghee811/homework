#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gurumdds/dcps.h>
#include <gurumdds/typesupport.h>

#include <Worker/MsgTypeSupport.h>

static Worker_MsgSeq* samples;
static dds_SampleInfoSeq* sampleinfos;

//리스너에 담을 콜백함수 생성
static void on_data_available(const dds_DataReader* a_reader) {
	dds_DataReader* reader = (dds_DataReader*)a_reader;

	//samples 리시브 성공하면 return dds_RETCODE_OK
	//samples에 받은 데이터가없으면 return dds_RETCODE_NO_DATA
	dds_ReturnCode_t ret = Worker_MsgDataReader_take(reader, samples, sampleinfos, 8, dds_ANY_SAMPLE_STATE, dds_ANY_VIEW_STATE, dds_ANY_INSTANCE_STATE);
	if(ret == dds_RETCODE_NO_DATA || ret != dds_RETCODE_OK)
		return;

	// SampleInfo 시퀀스의 길이만큼 돌면서 데이터를 가져온다.
	for(uint32_t i = 0; i < dds_SampleInfoSeq_length(sampleinfos); i++) {
		// 리시브된 샘플이 유효한지 검증한다.
		dds_SampleInfo* sampleinfo = dds_SampleInfoSeq_get(sampleinfos, i);
		if(!sampleinfo->valid_data)
			continue;

		// Dump a recevied samples one by one.
		Worker_Msg* sample = Worker_MsgSeq_get(samples, i);
		printf("Data received!\n");
		printf("name: %s\n", sample->name);
		printf("birth: %d\n", sample->birth);
		printf("team: %d\n", sample->team);
	}

	// Release memory no longer in use.
	dds_DataReader_return_loan(reader, samples, sampleinfos);
}


int main(int argc, char** argv) {
	dds_ReturnCode_t ret = dds_RETCODE_OK;
	dds_DomainParticipantFactory* factory = NULL;
	dds_DomainId_t domain_id = 0;
	dds_DomainParticipant* participant = NULL;

	// 도메인참가자 팩토리 인스턴스를 가져온다.
	factory = dds_DomainParticipantFactory_get_instance();
	if(factory == NULL)
		return 1;

	// 디폴트값은 도메인참가자의 QoS를 초기화하는데 사용된다.
	dds_DomainParticipantQos participant_qos;
	ret = dds_DomainParticipantFactory_get_default_participant_qos(factory, &participant_qos);
	if(ret != dds_RETCODE_OK)
		return 2;

	// 도메인참가자 엔티티를 생성한다. (마지막 두개 인자는 리스너와 관련이 있음)
	participant = dds_DomainParticipantFactory_create_participant(factory, domain_id, &participant_qos, NULL, 0);
	if(participant == NULL)
		return 3;

	// Get type name(Worker.msg)
	const char* type_name = Worker_MsgTypeSupport_get_type_name();
	if(type_name == NULL)
		return 4;
	// 도메인참가자 엔티티에 type을 등록
	ret = Worker_MsgTypeSupport_register_type(participant, type_name);
	if(ret != dds_RETCODE_OK)
		return 5;

	// 디폴트값은 토픽의 QoS를 초기화하는데 사용된다.
	dds_TopicQos topic_qos;
	ret = dds_DomainParticipant_get_default_topic_qos(participant, &topic_qos);
	if(ret != dds_RETCODE_OK)
		return 6;

	// 토픽엔티티 생성
	// 그리고 토픽의 이름을 'Workers' 로 지정한다.
	dds_Topic* topic = dds_DomainParticipant_create_topic(participant, "Workers", type_name, &topic_qos, NULL, 0);
	if(topic == NULL)
		return 7;

	// 디폴트값은 Subscriber의 QoS를 초기화하는데 사용된다.
	dds_SubscriberQos sub_qos;
	ret = dds_DomainParticipant_get_default_subscriber_qos(participant, &sub_qos);
	if(ret != dds_RETCODE_OK)
		return 8;
	// Subscriber 엔티티 생성
	dds_Subscriber* sub = dds_DomainParticipant_create_subscriber(participant, &sub_qos, NULL, 0);	  if(sub == NULL)
		return 9;

	//sub_qos 해제
	ret = dds_SubscriberQos_finalize(&sub_qos);
	if(ret != dds_RETCODE_OK)
		return 10;

	// 디폴트값은 DataReader의 QoS를 초기화하는데 사용된다.
	dds_DataReaderQos reader_qos;
	ret = dds_Subscriber_get_default_datareader_qos(sub, &reader_qos);
	if(ret != dds_RETCODE_OK)
		return 11;

	// 토픽의 Qos를 Subscriber의Qos에 덮어쓴다.
	ret = dds_Subscriber_copy_from_topic_qos(sub, &reader_qos, &topic_qos);
	if(ret != dds_RETCODE_OK)
		return 12;

	// DataReader엔티티를 생성하고 리스너는 이벤트 마스크와 함께 설정됩니다.
	dds_DataReaderListener drl = {0,};
	// 위에서 만든 콜백함수를 넣어준다.
	drl.on_data_available = on_data_available;
	dds_DataReader* dr = dds_Subscriber_create_datareader(sub, topic, &reader_qos, &drl, dds_DATA_AVAILABLE_STATUS);
	if(dr == NULL)
		return 13;
	ret = dds_DataReaderQos_finalize(&reader_qos);
	if(ret != dds_RETCODE_OK)
		return 14;
	ret = dds_TopicQos_finalize(&topic_qos);
	if(ret != dds_RETCODE_OK)
		return 15;

	// sample 시퀀스 생성
	samples = Worker_MsgSeq_create(8);
	if(samples == NULL)
		return 16;
	// sampleinfo 시퀀스 생성
	sampleinfos = dds_SampleInfoSeq_create(8);
	if(sampleinfos == NULL)
		return 17;

	puts("Press any key to exit"), getchar();

	//--------------------------------------
	// 정리
	//--------------------------------------
    Worker_MsgSeq_delete(samples);
	dds_SampleInfoSeq_delete(sampleinfos);

	dds_Time_t delay = { 1, 500 * 1000 * 1000 };
	dds_Time_sleep(&delay);

	// Terminate the GurumDDS middleware.
	// This API will send a messages that left a communication.
	// And Dispose of any resources you used in the GurumDDS.
	dds_DomainParticipantFactory_shutdown();

	return 0;
}
