// These reference may be useful for understanding tokens and message size
// https://platform.openai.com/docs/api-reference/chat/create#chat/create-max_tokens
// https://help.openai.com/en/articles/4936856-what-are-tokens-and-how-to-count-them
// https://platform.openai.com/docs/guides/chat/introduction

#define MAX_TOKENS 75
#define CHARS_PER_TOKEN 5  // Each token equates to roughly 4 chars, but to be conservative, lets add a small buffer
#define MAX_MESSAGE_LENGTH (MAX_TOKENS * CHARS_PER_TOKEN)
#define MAX_MESSAGES 20  // Everytime you send a message, it must inlcude all previous messages in order to respond with context

enum roles { sys, //system
             user,
             assistant };

// This is a HUGE chunk o' memory we need to allocate for all time
struct message {
  enum roles role;
  char content[MAX_MESSAGE_LENGTH];
} messages[MAX_MESSAGES];

void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}
