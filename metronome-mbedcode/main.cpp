#include "mbed.h"
#include "utils.hpp"
#include "stdio.h"

#include "EthernetInterface.h"
#include "frdm_client.hpp"

#include "metronome.hpp"



#define IOT_ENABLED

namespace active_low
{
  const bool on = false;
  const bool off = true;
}

RawSerial pc(USBTX, USBRX);

DigitalOut g_led_red(LED1);
DigitalOut g_led_green(LED2);
DigitalOut g_led_blue(LED3);

InterruptIn g_button_mode(SW3);
InterruptIn g_button_tap(SW2);

size_t prev_bpm =0,max_bpm=0, min_bpm=INT_MAX;
bool min_changed = 0, max_changed =0, bpm_changed =0;
uint8_t* buffIn = NULL;
uint32_t sizeIn;

Semaphore updates(0);

Ticker status_ticker;
void blinky(){
  
  g_led_green = !g_led_green;
  
  }
metronome metronome_object;


void metronome::start_timing(){
      m_timing = true;
      m_beat_count = 0;
      m_timer.start();
           
  }
  
void metronome::stop_timing(){
      
      m_timing = false;
      m_timer.stop();
    size_t curr_bpm=0;
      curr_bpm = metronome_object.get_bpm();
      pc.printf("BPM:- %d",curr_bpm);
      bpm_changed = 1;
      if(curr_bpm < min_bpm){
            min_bpm = curr_bpm;
            min_changed =1;
        }
      if(curr_bpm > max_bpm){
          max_bpm = curr_bpm;
          max_changed = 1;
        }
      if (curr_bpm == 0){
          g_led_green = active_low::off;
        }else{
          //pc.printf("Blinky Call");
          size_t tempo = 60/curr_bpm;
          status_ticker.attach(blinky,tempo);
          }      
}   
void metronome::tap(){
      
      m_beats[m_beat_count++] = m_timer.read_us();
      pc.printf("%d\n",m_timer.read_us());
      
    
} 
size_t metronome::get_bpm() const {
     int i=0;
     size_t bpm=0;
     if(m_beat_count < 4){
         return prev_bpm;
      }else{          
            for(i=0;i<m_beat_count-1;i++){
               bpm = bpm + m_beats[i+1]-m_beats[i];
              } 
            bpm = (size_t) bpm/(m_beat_count-1);
            prev_bpm = (bpm * 0.00006);
            pc.printf("BPM Calc:- %d",prev_bpm);
            return prev_bpm;
        }
} 

void on_mode()
{
  
  if(metronome_object.is_timing()){
        //Go to play mode
        //printf("playmode");
        g_led_red = active_low::off;
        g_led_green = active_low::on;
        metronome_object.stop_timing();
                
}else{
       //Go to learn mode
      // printf("learnmode");
       g_led_green = active_low::off;
       g_led_red = active_low::on;
       status_ticker.detach();
       metronome_object.start_timing();

      }
}

void on_tap()
{
    // Receive a tempo tap
    if(metronome_object.is_timing()){
          metronome_object.tap();
      }    
}


/*Frequency Class  and related resources*/
class FreqResource {
public:
    FreqResource() {
        
//        M2MObject* freq_object;
        freq_object = M2MInterfaceFactory::create_object("3318");
        M2MObjectInstance* freq_inst = freq_object->create_object_instance();


// Why is it blinking only 4 time? - Ram
        // there's not really an execute LWM2M ID that matches... hmm...
        M2MResource* setValue_res = freq_inst->create_dynamic_resource("5900", "Set Value",
            M2MResourceInstance::FLOAT, true);
        // we allow executing a function here...
        setValue_res->set_operation(M2MBase::GET_PUT_ALLOWED);
        //setValue_res->set_value_updated_function(value_updated_callback2(this, &FreqResource::handle_put_bpm));
        setValue_res->set_execute_function(execute_callback(this, &FreqResource::handle_put_bpm));
        // Completion of execute function can take a time, that's why delayed response is used
        setValue_res->set_delayed_response(true);
        
        //Max BPM
         M2MResource* maxBPM_res = freq_inst->create_dynamic_resource("5602", "Max BPM",
            M2MResourceInstance::FLOAT, true);
            
        // we allow executing a function here...
        maxBPM_res->set_operation(M2MBase::GET_ALLOWED);
    
        // Completion of execute function can take a time, that's why delayed response is used
       // maxBPM_res->set_delayed_response(true);
        
        //Min BPM
         M2MResource* minBPM_res = freq_inst->create_dynamic_resource("5601", "Min BPM",
            M2MResourceInstance::FLOAT, true);
        // we allow executing a function here...
        minBPM_res->set_operation(M2MBase::GET_ALLOWED);
    
        // Completion of execute function can take a time, that's why delayed response is used
        //minBPM_res->set_delayed_response(true);
        
        //Reset Min/Max
         M2MResource* resetBPM_res = freq_inst->create_dynamic_resource("5605", "Reset Min/Max BPM",
            M2MResourceInstance::STRING, true);
        // we allow executing a function here...
        resetBPM_res->set_operation(M2MBase::POST_ALLOWED);        
        resetBPM_res->set_execute_function(execute_callback(this, &FreqResource::handle_reset_minMax_bpm));
        // Completion of execute function can take a time, that's why delayed response is used
        resetBPM_res->set_delayed_response(true);
        
        //Units
         M2MResource* unitsBPM_res = freq_inst->create_dynamic_resource("5701", "Units",
            M2MResourceInstance::STRING, true);
        // we allow executing a function here...
        unitsBPM_res->set_operation(M2MBase::GET_ALLOWED);
        // Completion of execute function can take a time, that's why delayed response is used
        //unitsBPM_res->set_delayed_response(true);
        unitsBPM_res->set_value((uint8_t*)"BPM",3);
    }
    

    ~FreqResource() {
        
    }

   void handle_put_bpm(void *argument){
        
        status_ticker.detach();
        M2MObjectInstance* inst = freq_object->object_instance();
        M2MResource* setValue_res = inst->resource("5900");

        // values in mbed Client are all buffers, and we need a vector of int's
        setValue_res->get_value(buffIn, sizeIn);
        
        int temp=0;
            for(int i=0;i<sizeIn;i++){
            temp=temp*10+(int)buffIn[i]-48;
            }
        prev_bpm=temp;
            
      if(prev_bpm < min_bpm)
      {
            min_bpm = prev_bpm;
            min_changed =1;
        }
      if(prev_bpm > max_bpm)
      {
          max_bpm = prev_bpm;
          max_changed = 1;
        }
        size_t tempo = 60/prev_bpm;
        status_ticker.attach(blinky,tempo);
          
        bpm_changed = 1; 
   }
   
   void handle_set_bpm(){
       
      M2MObjectInstance* inst = freq_object->object_instance();
        M2MResource* setValue_res = inst->resource("5900");
        
        char buffer[20];
        int size = sprintf(buffer,"%d",prev_bpm);
        setValue_res->set_value((uint8_t*)buffer, size);
      
      }
      
    void handle_set_min_bpm(){
      //Send a notification
      M2MObjectInstance* inst = freq_object->object_instance();
        M2MResource* minBPM_res = inst->resource("5601");
        pc.printf("%d",min_bpm);
        char buffer[20];
        int size = sprintf(buffer,"%d",min_bpm);
        minBPM_res->set_value((uint8_t*)buffer, size);
       
      
      }
    void handle_set_max_bpm(){
      //Send a notification
      M2MObjectInstance* inst = freq_object->object_instance();
        M2MResource* maxBPM_res = inst->resource("5602");
        
        char buffer[20];
        int size = sprintf(buffer,"%d",max_bpm);
        maxBPM_res->set_value((uint8_t*)buffer, size);
       
      
      }
    
    void handle_reset_minMax_bpm(void *argument){
      //Handle request to reset the min_max values to zero and 1000000 each
           
         M2MObjectInstance* inst = freq_object->object_instance();
         M2MResource* minBPM_res = inst->resource("5601");
         M2MResource* maxBPM_res = inst->resource("5602");

               minBPM_res->set_value(0);
               maxBPM_res->set_value(0);               
          }       
      
    M2MObject* get_object() {
        return freq_object;
    }

private:
    M2MObject* freq_object;
};

int main()
{
  // Seed the RNG for networking purposes
    unsigned seed = utils::entropy_seed();
    srand(seed);

  // LEDs are active LOW - true/1 means off, false/0 means on
  // Use the constants for easier reading
    g_led_red = active_low::off;
    g_led_green = active_low::off;
    g_led_blue = active_low::off;
    

  // Button falling edge is on push (rising is on release)
    g_button_mode.fall(&on_mode);
    g_button_tap.fall(&on_tap);

#ifdef IOT_ENABLED
  // Turn on the blue LED until connected to the network
   // g_led_blue = active_low::off;


  // Need to be connected with Ethernet cable for success
    EthernetInterface ethernet;
    if (ethernet.connect() != 0)
        return 1;

  // Pair with the device connector
    frdm_client client("coap://api.connector.mbed.com:5684", &ethernet);
    if (client.get_state() == frdm_client::state::error)
        return 1;

  // The REST endpoints for this device
  // Add your own M2MObjects to this list with push_back before client.connect()
    M2MObjectList objects;
    
    //Frequency device definition and instance definition
    FreqResource freq_resource;         

    M2MDevice* device = frdm_client::make_device();
    objects.push_back(device);
    objects.push_back(freq_resource.get_object());
    
  // Publish the RESTful endpoints
    client.connect(objects);

  // Connect complete; turn off blue LED forever
    //g_led_blue = active_low::on;
    
#endif

    while (true)
    {
      //updates.wait(25000);
#ifdef IOT_ENABLED
        if (client.get_state() == frdm_client::state::error)
            break;
#endif

        // Insert any code that must be run continuously here
        if(bpm_changed){
           bpm_changed = 0;
           freq_resource.handle_set_bpm();
          }
          
        if(min_changed) {
           min_changed = 0;
            freq_resource.handle_set_min_bpm();
        }
        if(max_changed) {
           max_changed = 0;
            freq_resource.handle_set_max_bpm();
        }
        
    }

#ifdef IOT_ENABLED
    client.disconnect();
#endif
    return 1;
}
