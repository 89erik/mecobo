// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "emEvolvableMotherboard.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include "mecohost.h"
#include "../mecoprot.h"
#include <map>
#include <queue>
#include <thread>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

using namespace  ::emInterfaces;

class emEvolvableMotherboardHandler : virtual public emEvolvableMotherboardIf {

  std::thread runThread;

  int64_t time;
  std::map<int, std::queue<emSequenceItem>> pinSeq;
  std::map<int, std::vector<uint32_t>> rec;
  //std::vector<int> recPins;
  std::vector<emSequenceItem> itemsInFlight;

  public:
  emEvolvableMotherboardHandler() {
    // Your initialization goes here
    time = 0;
  }

  int32_t ping() {
    // Your implementation goes here
    printf("ping\n");
    return 0;
  }

  void setLED(const int32_t index, const bool state) {
    state ? moboSetLed(index, 0) : moboSetLed(index, 1);
    std::cout << "Led " << index << "is " << state << std::endl;
  }

  void getMotherboardID(std::string& _return) {
    // Your implementation goes here
    printf("getMotherboardID\n");
  }

  void getMotherboardState(std::string& _return) {
    // Your implementation goes here
    printf("getMotherboardState\n");
  }

  void getLastError(std::string& _return) {
    // Your implementation goes here
    printf("getLastError\n");
  }

  bool reset() {
    // Your implementation goes here
    resetAllPins();
    printf("reset\n");
    return true;
  }

  bool reprogramme(const std::string& bin, const int32_t length) {
    // Your implementation goes here
    printf("reprogramme\n");
    return true;
  }

  void getDebugState(emDebugInfo& _return) {
    // Your implementation goes here
    printf("getDebugState\n");
  }

  void clearSequences() {
    // Your implementation goes here
    
    printf("clearSequences\n");
    pinSeq.clear();
  }

  void runSequences() {
  
    auto start = std::chrono::system_clock::now();
    auto lastGetBuffer = std::chrono::system_clock::now(); 
    bool done = false;
    for(auto p : rec) {
      p.second.clear();
    }
    int64_t lastEndTime = -1;

    while(!done) {
      //Iterate all the queues, check if it's time to do a sequence action.
      for(auto k : pinSeq) {
        //Peek queue
        if(pinSeq[k.first].size() > 0) {
          auto item = pinSeq[k.first].front();
          auto now = std::chrono::system_clock::now();

          if(item.endTime > lastEndTime) {
            //This is the last item, and we must wait for it to finish.
            lastEndTime = item.endTime;
          }
          
          //If an item's start time has passed.
          if(item.startTime <= std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count()) {
            itemsInFlight.push_back(item); 
            std::cout << "Playing item on pin " << k.first << " scheduled at" << item.startTime << "Time is: " <<  std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() << std::endl;
           
            setupItem(item);
            pinSeq[k.first].pop(); //remove element from queue.
          }
        }
      }

      
      //check if we have items in flight that it's time to kill or do something with
      auto now = std::chrono::system_clock::now();
      std::vector<sampleValue> samples;
      for(int i = 0; i < (int)itemsInFlight.size(); i++) {
        emSequenceItem item = itemsInFlight[i];

        //Sample buffer fetching.
        if(item.operationType == emSequenceOperationType::type::RECORD) {
          //auto now = std::chrono::system_clock::now();
          if(10000 <= std::chrono::duration_cast<std::chrono::microseconds>(now - lastGetBuffer).count()) {
            lastGetBuffer = now;
            //Poke the board for sample buffers for the 
            //pins that we have selected for recording.
            std::vector<sampleValue> samples;
            getSampleBuffer(samples);
            for(auto s : samples) {
              //std::cout << s.pin << ":" << s.sampleNum << std::endl;
              rec[s.pin].push_back(s.value);
            }
          }
        }



        if(item.endTime <= std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count()) {
          std::cout << "Killing item ending at " << item.endTime << ". Time is: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() << std::endl;
          switch(item.operationType) {
            case emSequenceOperationType::type::RECORD:
              break;
            default:
              break;
          }
          //remove
          itemsInFlight.erase(itemsInFlight.begin() + i);
        }


      }


      //Check if queues are empty and last item has finished.
      done = true;
      for(auto q : pinSeq) {
        if(q.second.size() > 0) {
          done = false;
        }
        if(itemsInFlight.size() > 0) {
          done = false;
        }
      }

      auto n = std::chrono::system_clock::now();
      if(lastEndTime >= std::chrono::duration_cast<std::chrono::milliseconds>(n - start).count()) {
        done = false;
      }

    }
    std::cout << "Sequence done, all qeueues empty." << std::endl;
  } 


  void stopSequences() {
    // Your implementation goes here
    printf("stopSequences\n");
  }

  void joinSequences() {
    // Your implementation goes here
    printf("joinSequences\n");
  }

  void appendSequenceAction(const emSequenceItem& Item) {
    //TODO: Lots of error checking and all that jazzy.
    std::cout << "Appending action" << std::endl;

    //If it starts at time = 0, just set it up immediatly.
    if((Item.operationType != emSequenceOperationType::type::RECORD) &&
        Item.startTime == 0) {
      setupItem(Item); 
      itemsInFlight.push_back(Item);
    } else  {
      pinSeq[Item.pin].push(Item);
    }
  }

  void getRecording(emWaveForm& _return, const int32_t srcPin) {
    // Your implementation goes here
    std::vector<int32_t> v;
    std::vector<sampleValue> samples;

    std::cout << "There are " << rec[srcPin].size() << "samples for pin " << srcPin << std::endl;
    for(auto s : rec[srcPin]) {
      //std::cout << s.sampleNum << std::endl;
      v.push_back(s);
    }

    emWaveForm r;
    r.SampleCount = v.size();
    r.Samples = v;
    _return = r;

    rec[srcPin].clear();
  }


  void clearRecording(const int32_t srcPin) {
    // Your implementation goes here
    printf("clearRecording\n");
  }

  int32_t getTemperature() {
    // Your implementation goes here
    printf("getTemperature\n");
    return 0;
  }

  void setLogServer(const emLogServerSettings& logServer) {
    // Your implementation goes here
    printf("setLogServer\n");
  }

  private:
  std::string stringRepItem(emSequenceItem & em) 
  {
    std::string ret;
    ret.append("-- ");
    return ret;
  }
  /*
    void sanitizeItem(emSequenceItem & em) {
      switch(item.operationType) {
        case emSequenceOperationType::type::PWM:
          break;
        default:
          break;
      } 
    }
    */
  void setupItem(emSequenceItem item) {
    double period; //= 1.0f/(double)s.frequency;
    int32_t duty; // = period * (25*1000000);
    int32_t aduty; // = period * (25*1000000);
    uint32_t sampleDiv = ((50*1000000)/(8.0d*(double)item.frequency));
    emException err;

    switch(item.operationType) {
      case emSequenceOperationType::type::CONSTANT:
        std::cout << "CONSTANT voltage: " << item.amplitude << "on pin " << item.pin << std::endl;
        setPin((FPGA_IO_Pins_TypeDef)item.pin, item.amplitude, 0, 0x1, 0x0);
        startConstOutput((FPGA_IO_Pins_TypeDef)item.pin);
        break;
      case emSequenceOperationType::type::PREDEFINED:

        //Since it's predefined, we have a waveFormType
        if(item.waveFormType == emWaveFormType::PWM) {
          period = 1.0d/(double)item.frequency;

          duty =  (item.cycleTime/100.0d)*(period * (50*1000000));
          aduty = ((100.0f - item.cycleTime)/100.0d) * period * (50*1000000);


          if(item.frequency < 382) {
            std::cout << "Throwing error, duty is " << duty << std::endl;
            //std::string reason;
            //reason << "Frequency too low: " << item.frequency << ", minimum 400Hz please." << std::endl;
            err.Reason = "too low freq"; //reason;
            err.Source = "emMotherboard sequencer";
            throw err;
            break;
          }
          std::cout << "PREDEFINED PWM: Freq:" << item.frequency << "Duty" << duty << "Antiduty: " << aduty << std::endl;
          setPin((FPGA_IO_Pins_TypeDef)item.pin, (uint32_t)duty, (uint32_t)aduty, 0x1, 0x0);
          startOutput((FPGA_IO_Pins_TypeDef)item.pin);
        }
        break;



      case emSequenceOperationType::type::RECORD:
        std::cout << "RECORD. start: " << item.startTime << "end: " << item.endTime <<"   Freq: " << item.frequency << "Gives sample divisor:" << sampleDiv << std::endl;
        if(sampleDiv <= 1) {
          err.Reason = "samplerate too high";
          err.Source = "emMotherboard";
          throw err;
          break;
        } else if (sampleDiv > 65535) {
          err.Reason = "samplerate too low";
          err.Source = "emMotherboard";
          throw err;
          break;
        }
        setPin((FPGA_IO_Pins_TypeDef)item.pin, 1, 1, 1, sampleDiv);
        startInput((FPGA_IO_Pins_TypeDef)item.pin);
        break;
      default:
        break;
    }

  }
};

int main(int argc, char **argv) {

  uint32_t progFpga = 0;
  //Command line arguments
  if (argc > 1) {
    for(int i = 0; i < argc; i++) {
      if(strcmp(argv[i], "-f") == 0) {
        progFpga = 1;
      }
    }
  }
  

  int port = 9090;

  std::cout << "Starting USB..." << std::endl;
  startUsb();
  std::cout << "Done!" << std::endl;

  if(progFpga) {
    programFPGA("mecobo.bin");
  }


  shared_ptr<emEvolvableMotherboardHandler> handler(new emEvolvableMotherboardHandler());
  shared_ptr<TProcessor> processor(new emEvolvableMotherboardProcessor(handler));
  shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

  std::cout << "Starting thrift server. (Silence ensues)." << std::endl;
  server.serve();

  std::cout << "Stopping USB..." << std::endl;
  stopUsb();
  std::cout << "Done!" << std::endl;

  return 0;
}



