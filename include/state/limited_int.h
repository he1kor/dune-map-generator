#pragma once

class LimitedInt{
    public:
        LimitedInt();
        bool setValue(int value);
        LimitedInt(int min_value, int max_value, int default_value);
        int getMaxLimit();
        int getMinLimit();
        int getValue() const;
    private:
        int value;
        int max_value;
        int min_value;
};
