#pragma once

/**
 * @class IMode
 * @brief Abstract interface class representing a generic operation mode of the PianoHand.
 */
class IMode {
  public:
    virtual ~IMode() = default;

    /**
     * @brief Prints mode selection guidelines or menu layout on the LCD.
     */
    virtual void print() = 0;

    /**
     * @brief Executes the active mode routine (processing/playback).
     */
    virtual void play() {}
};
