//
//  main.m
//  CBForest
//
//  Created by Jens Alfke on 3/26/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//

#import <CBForest/CBForest.h>


int main(int argc, const char * argv[])
{
    @autoreleasepool {
        [CBForest class];
        NSLog(@"CBForest runs!");
    }
    return 0;
}
