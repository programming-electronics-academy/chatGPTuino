#define MAX_CHARS_PER_LINE 20
#define MAX_ROWS 20
//4 2 3 9
char response[] = "Life is the existence of an individual, potentially exhibiting consciousness, and sapience, or the exhibition of a specified set of characteristics. It is the aspect of existence that processes, measures, an";

char responseMatrix[MAX_ROWS][MAX_CHARS_PER_LINE] = {};
int responseLength = 207;

int chatIndex = 0;


int lengthOfToken(int startIndex, int stopIndex, char charArray[])
{
  for(int i = 0; i < stopIndex-startIndex; i++)
  {
    if(charArray[i + startIndex] == ' '){
      return i;
    }
  }
  return -1;
}

void createResponseMatrix(char input[], char output[][], int inputLen, byte outputRows)
{
    int inputIndex = 0;
    int outCol = 0;
    int outRow = 0;
    
    //adjust chat index for newlines at beginnning of response
    while(input[inputIndex] == '\n'){
      inputIndex++;
    }

    while (inputIndex < inputLen) {

      //Get length of next token
      byte lenOfNextToken = lengthOfToken(inputIndex, inputLen, input);


      //If next word extends beyond current line, go to next line
      // if (lenOfNextToken + chatIndex >= (MAX_CHAR_PER_LINE - 2) * (lineNum - 1)) {
      if (lenOfNextToken + outCol >= MAX_CHAR_PER_LINE) {
        outRow++;
        outCol = 0;
      }

      for (int i = inputIndex; i < inputIndex + lenOfNextToken + 1; i++) {
        output[outRow][i] = input[i];
      }

      inputIndex += lenOfNextToken + 1;

    }


  }

}

void setup() {
  Serial.begin(9600);

  int distance = lengthOfToken(chatIndex, responseLength, response);
  Serial.print("distance 1 => ");
  Serial.println(distance);
  
  chatIndex += distance + 1;
  distance = lengthOfToken(chatIndex, responseLength, response);
  Serial.print("distance 2 => ");
  Serial.println(distance);
  
  chatIndex += distance + 1;
  distance = lengthOfToken(chatIndex, responseLength, response);
  Serial.print("distance 3 => ");
  Serial.println(distance);
  
  chatIndex += distance + 1;
  distance = lengthOfToken(chatIndex, responseLength, response);
  Serial.print("distance 4 => ");
  Serial.println(distance);

  createResponseMatrix(response, responseMatrix, responseLength, byte outputRows)


}

void loop() {
  // put your main code here, to run repeatedly:

}
