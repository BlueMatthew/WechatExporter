//
//  ByteArrayLocater.h
//  WechatExporter
//
//  Created by Matthew on 2020/10/19.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <vector>

#ifndef ByteArrayLocater_h
#define ByteArrayLocater_h

class ByteArrayLocater
{
public:
    std::vector<int> locate(const unsigned char* data, int length, const unsigned char* candidate, int candidateLength)
    {
        std::vector<int> positions;
        if (isEmptyLocate(data, length, candidate, candidateLength))
            return positions;

        for (int i = 0; i < length; i++)
        {
            if (!isMatch(data, length, i, candidate, candidateLength))
                continue;

            positions.push_back(i);
        }

        return positions;
    }
    
    std::vector<std::pair<int, int>> locatePair(const unsigned char* data, int length, const unsigned char* startCandidate, int startCandidateLength, const unsigned char* endCandidate, int endCandidateLength)
    {
        std::vector<std::pair<int, int>> positions;
        if (isEmptyLocate(data, length, startCandidate, startCandidateLength))
            return positions;

        for (int i = 0; i < length;)
        {
            if (!isMatch(data, length, i, startCandidate, startCandidateLength))
            {
                i++;
                continue;
            }
            
            int j = i + startCandidateLength;
            
            for (; j < length;)
            {
                if (!isMatch(data, length, j, endCandidate, endCandidateLength))
                {
                    j++;
                    continue;
                }
                
                positions.push_back(std::make_pair(i, j));
                break;
            }
            
            if (j == length)
            {
                // No endTag found
                break;
            }
            
            i = j + endCandidateLength;
        }

        return positions;
    }

    bool isMatch(const unsigned char* data, int length, int position, const unsigned char* candidate, int candidateLength)
    {
        if (candidateLength > (length - position))
            return false;

        for (int i = 0; i < candidateLength; i++)
            if (data[position + i] != candidate[i])
                return false;

        return true;
    }

    bool isEmptyLocate(const unsigned char* data, int length, const unsigned char* candidate, int candidateLength)
    {
        return data == NULL
            || candidate == NULL
            || length == 0
            || candidateLength == 0
            || candidateLength > length;
    }

    
};

#endif /* ByteArrayLocater_h */
