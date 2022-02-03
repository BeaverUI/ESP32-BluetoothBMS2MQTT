// EasyInterrupt - software interrupts

bool InterruptPending(unsigned long *prev_millis, unsigned int period, int mode){
  // mode = 0: approximate mode without catch-up
  // mode = 1: exact mode without catch-up
  // mode = 2: exact mode with catch-up
  // note: overflow is handled correctly and exactly (tested)
  
  if( (millis()-(*prev_millis) > period) || (millis()-(*prev_millis) < 0)){
    // trigger detected
    switch(mode){
      default:
      case 0:
        // approximate mode without catch-up
        *prev_millis=millis();
        break;
        
      case 1:
        // exact mode without catch-up
        while(millis()-(*prev_millis) > period){ // unwind
          *prev_millis=*prev_millis+period;
        }
        break;
        
      case 2:
        // exact mode with catch-up
        *prev_millis=*prev_millis+period;
        break;
    }
      
    return true;
  }else{
    return false;
  }
}
