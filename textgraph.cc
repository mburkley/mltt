#include <stdio.h>
#include <string.h>

#include "textgraph.h"

TextGraph::TextGraph()
{
    memset (_data, ' ', GRAPH_HEIGHT*GRAPH_WIDTH);
    _column = 0;
}

int TextGraph::_range (int x)
{
    x = GRAPH_HEIGHT * (x - _min)/ (_max - _min);
    // x = (x + 32000) /4000;
    if (x<0) x = 0;
    if (x>15) x = 15;
    return x;
}

void TextGraph::add (int sample, int min, int max, int zero)
{
    if (_column == GRAPH_WIDTH)
        return;

    sample = _range(sample);
    min = _range(min);
    max = _range(max);
    zero = _range(zero);

    _data[_column][min] = 'L';
    _data[_column][max] = 'H';
    _data[_column][zero] = 'Z';
    _data[_column][sample] = '+';
    _column++;
}

void TextGraph::remove (int count)
{
    if (count > _column)
        _column = 0;
    else
        _column -= count;

    for (int y = 0; y < GRAPH_HEIGHT; y++)
        for (int x = 0; x < GRAPH_WIDTH; x++)
        {
            if (x < _column)
                _data[x][y] = _data[x+count][y];
            else
                _data[x][y] = ' ';
        }
}

void TextGraph::vertical (char c)
{
    if (_column == GRAPH_WIDTH)
        return;

    for (int y = 0; y < GRAPH_HEIGHT; y++)
        _data[_column][y] = c;

    _column++;
}

void TextGraph::draw (void)
{
    // for (int y = 0; y < GRAPH_HEIGHT; y++)
    for (int y = GRAPH_HEIGHT-1; y >= 0; y--)
    {
        // printf ("                        ");

        for (int x = 0; x < _column; x++)
            printf ("%c", _data[x][y]);

        printf ("\n");
    }

    // graphCol = 0;
    // memset (graph, ' ', GRAPH_HEIGHT*GRAPH_WIDTH);
}


