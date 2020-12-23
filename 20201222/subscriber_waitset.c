#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gurumdds/dcps.h>
#include <gurumdds/typesupport.h>

#include <Worker/MsgTypeSupport.h>

int main(int argc, char** argv) {
	dds_ReturnCode_t ret = dds_RETCODE_OK;
	dds_DomainParticipantFactory* factory = NULL;
	dds_DomainId_t domain_id = 0;
	dds_DomainParticipant* participant = NULL;

	// Get entity of DomainParticipantFactory.
	factory = dds_DomainParticipantFactory_get_instance();
	if(factory == NULL)
		return 1;

	// A default value is used to initialize a QoS of DomainParticipant.
	dds_DomainParticipantQos participant_qos;
	ret = dds_DomainParticipantFactory_get_default_participant_qos(factory, &participant_qos);
	if(ret != dds_RETCODE_OK)
		return 2;
	// Create a DomainParticipant entity.
	participant = dds_DomainParticipantFactory_create_participant(factory, domain_id, &participant_qos, NULL, 0);
	if(participant == NULL)
		return 3;

	// Get type name(Worker.msg)
	const char* type_name = Worker_MsgTypeSupport_get_type_name();
	if(type_name == NULL)
		return 4;
	// Register type with DomainParticipant entity.
	ret = Worker_MsgTypeSupport_register_type(participant, type_name);
	if(ret != dds_RETCODE_OK)
		return 5;

	// A default value is used to initialize a QoS of Topic.
	dds_TopicQos topic_qos;
	ret = dds_DomainParticipant_get_default_topic_qos(participant, &topic_qos);
	if(ret != dds_RETCODE_OK)
		return 6;
	// Create a Topic entity.
	// And specify a Topic name as 'Workers'
	dds_Topic* topic = dds_DomainParticipant_create_topic(participant, "Workers", type_name, &topic_qos, NULL, 0);
	if(topic == NULL)
		return 7;

	// A default value is used to initialize a QoS of Subscriber.
	dds_SubscriberQos sub_qos;
	ret = dds_DomainParticipant_get_default_subscriber_qos(participant, &sub_qos);
	if(ret != dds_RETCODE_OK)
		return 8;
	// Create a Subscriber entity.
	dds_Subscriber* sub = dds_DomainParticipant_create_subscriber(participant, &sub_qos, NULL, 0);
	if(sub == NULL)
		return 9;

	ret = dds_SubscriberQos_finalize(&sub_qos);
	if(ret != dds_RETCODE_OK)
		return 10;

	// A default value is used to initialize a QoS of DataReader.
	dds_DataReaderQos reader_qos;
	ret = dds_Subscriber_get_default_datareader_qos(sub, &reader_qos);
	if(ret != dds_RETCODE_OK)
		return 11;
	// Overwrite QoS of Subscriber with Qos of Topic.
	ret = dds_Subscriber_copy_from_topic_qos(sub, &reader_qos, &topic_qos);
	if(ret != dds_RETCODE_OK)
		return 12;
	// Create a DataReader entity.
	dds_DataReader* dr = dds_Subscriber_create_datareader(sub, topic, &reader_qos, NULL, 0);
	if(dr == NULL)
		return 13;
	ret = dds_DataReaderQos_finalize(&reader_qos);
	if(ret != dds_RETCODE_OK)
		return 14;
	ret = dds_TopicQos_finalize(&topic_qos);
	if(ret != dds_RETCODE_OK)
		return 15;

	// Create a sequence for samples.
	Worker_MsgSeq* samples = Worker_MsgSeq_create(8);
	if(samples == NULL)
		return 16;
	// Create a sequence for sampleinfos.
	dds_SampleInfoSeq* sampleinfos = dds_SampleInfoSeq_create(8);
	if(sampleinfos == NULL)
		return 17;

	// 등록된 컨디션중에 하나라도 충족되면 Waitset은 waiting에서 빠져나온다.
    // active를 생성한다.(컨디션들을 담을 시퀀스) 인자는 캐퍼시티값임.  컨디션이 추가되면 자동으로 늘어남.
	dds_ConditionSeq* active = dds_ConditionSeq_create(1);
    // waitset이 기다릴 최대시간을 만든다다.
	dds_Duration_t timeout = { 10, 0 };
	// waitset을 생성한다.
	dds_WaitSet* waitset = dds_WaitSet_create();
	// Read컨디션을 생성한다. (컨디션은 DataReader에 종속된다.)
	dds_ReadCondition* cond_read = dds_DataReader_create_readcondition(dr, dds_ANY_SAMPLE_STATE,
			dds_ANY_VIEW_STATE, dds_ANY_INSTANCE_STATE);
	// 생성한 Read컨디션을 waitset에 attach한다.
	dds_WaitSet_attach_condition(waitset, (dds_Condition*)cond_read);
	// 트리거가 켜지거나 최대시간에 도달할때까지 기다린다.
	dds_WaitSet_wait(waitset, active, &timeout);

	if(dds_ConditionSeq_length(active)) {
		//samples을 받았다면 active의 길이가 1이상이되고 return  dds_RETCODE_OK
		ret = Worker_MsgDataReader_take(dr, samples, sampleinfos, 8, dds_ANY_SAMPLE_STATE, dds_ANY_VIEW_STATE, dds_ANY_INSTANCE_STATE);
	
	    for(uint32_t i = 0; i < dds_SampleInfoSeq_length(sampleinfos); i++){
			dds_SampleInfo* sampleinfo = dds_SampleInfoSeq_get(sampleinfos, i);
			if(!sampleinfo->valid_data)
				continue;

			Worker_Msg* sample = Worker_MsgSeq_get(samples, i);
			printf("Data received!\n");
			printf("name: %s\n", sample->name);
			printf("birth: %d\n", sample->birth);
			printf("team: %d\n", sample->team);
		}

		//Release memory no longer in use.
		dds_DataReader_return_loan(dr, samples, sampleinfos);
	}

	//waitset에 attach해놨던 컨디션을 detach해준다.
	dds_WaitSet_detach_condition(waitset, (dds_Condition*)cond_read);
	//컨디션을 DataReader에서 제거한다.
	dds_DataReader_delete_readcondition(dr, cond_read);
    //waitset을 제거한다.
	dds_WaitSet_delete(waitset);
	//active를 제거한다.
	dds_ConditionSeq_delete(active);

	
	//worker 샘플시퀀스 해제
	Worker_MsgSeq_delete(samples);
	//sampleinfo 시퀀스 해제
	dds_SampleInfoSeq_delete(sampleinfos);

	dds_Time_t delay = { 1, 500 * 1000 * 1000 };
	dds_Time_sleep(&delay);

	// Terminate the GurumDDS middleware.
	// This API will send a messages that left a communication.
	// And Dispose of any resources you used in the GurumDDS.
	dds_DomainParticipantFactory_shutdown();

	return 0;
}
