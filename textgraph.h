#ifndef __TEXTGRAPH_H
#define __TEXTGRAPH_H

#define GRAPH_WIDTH     100
#define GRAPH_HEIGHT    16

class TextGraph
{
public:
    TextGraph();
    void add (int sample, int min, int max, int zero, int state);
    void remove (int count);
    void vertical (void);
    void draw (void);
private:
    int _column;
    char _data[GRAPH_WIDTH][GRAPH_HEIGHT];
    int _range (int x);
};

#endif

