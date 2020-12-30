#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gurumdds/dcps.h>
#include <gurumdds/typesupport.h>

#include <Worker/MsgTypeSupport.h>
static void on_liveliness_changed(const dds_DataReader* the_reader, const dds_LivelinessChangedStatus* status){
	printf("liveness changed! %d %d\n",status->alive_count, status->not_alive_count);
}

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
	// 리스너 생성
	dds_DataReaderListener drl = {0,};
	drl.on_liveliness_changed = on_liveliness_changed;
	// Liveliness Qos 설정
	reader_qos.liveliness.kind = dds_MANUAL_BY_TOPIC_LIVELINESS_QOS;
	reader_qos.liveliness.lease_duration.sec = 2;
	reader_qos.liveliness.lease_duration.nanosec = 0;
	// Create a DataReader entity.
	dds_DataReader* dr = dds_Subscriber_create_datareader(sub, topic, &reader_qos, &drl, dds_LIVELINESS_CHANGED_STATUS);
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

	while(true) {
		// If samples was recevied successfully, take API will return dds_RETCODE_OK.
		// But if there are no recevied samples, take API will return dds_RETCODE_NO_DATA.
		ret = Worker_MsgDataReader_take(dr, samples, sampleinfos, 8, dds_ANY_SAMPLE_STATE, dds_ANY_VIEW_STATE, dds_ANY_INSTANCE_STATE);
		// If there are no recevied samples, continue next loop.
		if(ret == dds_RETCODE_NO_DATA || ret != dds_RETCODE_OK) {
			dds_Time_t delay = { 1, 0 };
			dds_Time_sleep(&delay);
			continue;
		}

		for(uint32_t i = 0; i < dds_SampleInfoSeq_length(sampleinfos); i++) {
			// Verify that a recevied sample is valid.
			dds_SampleInfo* sampleinfo = dds_SampleInfoSeq_get(sampleinfos, i);
			if(!sampleinfo->valid_data){
				printf("invalid data!\n");
				continue;
			}
			// Dump a recevied samples one by one.
			Worker_Msg* sample = Worker_MsgSeq_get(samples, i);
			printf("Data received!\n");
			printf("name: %s\n", sample->name);
			printf("birth: %d\n", sample->birth);
			printf("team: %d\n", sample->team);
		}

		// Release memory no longer in use.
		dds_DataReader_return_loan(dr, samples, sampleinfos);

		//break;
	}

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
