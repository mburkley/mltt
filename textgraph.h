#ifndef __TEXTGRAPH_H
#define __TEXTGRAPH_H

#define GRAPH_WIDTH     100
#define GRAPH_HEIGHT    16

class TextGraph
{
public:
    TextGraph();
    void limits (int min, int max) { _min = min; _max = max; };
    void add (int sample, int min, int max, int zero);
    void remove (int count);
    void clear () { remove (_column); }
    void vertical (char c = '|');
    void draw (void);
private:
    int _column;
    char _data[GRAPH_WIDTH][GRAPH_HEIGHT];
    int _range (int x);
    int _min;
    int _max;
};

#endif

