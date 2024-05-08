#ifndef DOORSTATE_H
#define DOORSTATE_H

class DoorState {
  public:
    DoorState();
    void lock();
    bool unlock(String code);
    bool locked();
    bool hasCode();
    void setCode(String newCode);

  private:
    void setLock(bool locked);
    bool _locked;
};

#endif /* DOORSTATE_H */
