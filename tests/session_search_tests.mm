#import "SessionDataSource.h"
#include <cassert>
int main() { @autoreleasepool {
SessionItem *a=[SessionItem new], *b=[SessionItem new], *c=[SessionItem new];
a.displayName=@"张三"; a.sessionUsrName=@"wxid_ABC"; a.usrName=@"owner"; a.checked=YES;
b.displayName=@"张三跑步群"; b.sessionUsrName=@"group@chatroom"; b.usrName=@"owner"; b.checked=YES;
c.displayName=nil; c.sessionUsrName=@"other"; c.usrName=@"owner"; c.checked=YES;
SessionDataSource *source=[SessionDataSource new];
[source setValue:@[a,b,c] forKey:@"m_allSessions"];
[source filterSessions:@"张三"]; assert([source numberOfRowsInTableView:nil]==2);
[source checkAllSessions:NO]; assert(!a.checked && !b.checked && c.checked);
[source filterSessions:@"  abc  "]; assert([source numberOfRowsInTableView:nil]==1);
[source updateCheckStateAtRow:0]; assert(a.checked);
std::map<std::string,std::map<std::string,void *>> selected;
[source getSelectedUserAndSessions:selected];
assert(selected.size()==1 && selected["owner"].size()==1 && selected["owner"].count("wxid_ABC"));
[source filterSessions:@"missing"]; assert([source numberOfRowsInTableView:nil]==0); assert([source selectionState]==NSControlStateValueOff);
[source filterSessions:@""]; assert([source numberOfRowsInTableView:nil]==3); assert(a.checked && !b.checked && c.checked); assert([source selectionState]==NSControlStateValueMixed);
// Reused cells must clear stale text when malformed UTF-8 produces nil.
NSTableCellView *cell = [NSTableCellView new];
NSTextField *field = [NSTextField new]; cell.textField = field;
const char invalid[] = {(char)0xff, 0};
c.lastMessage = [NSString stringWithUTF8String:invalid];
assert(c.lastMessage == nil);
for (NSString *column in @[@"columnName", @"columnUser", @"columnLastMsg"]) {
    field.stringValue = @"previous row";
    [source bindCellView:cell atRow:2 andColumnId:column];
    assert([field.stringValue isEqualToString:@""]);
}
puts("PASS: Chinese names, groups, case-insensitive ID, whitespace, nil name, selection preservation, filtered export, empty results, clear");
} }
