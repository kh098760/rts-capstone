# Explaination
**Queue depth**: how did you size it? Compute the worst-case burst your producer can deliver before the consumer catches up. Show the math.

-  I sized it to queue up to 10 elements. This is because at 20hz (50ms), if at worst case the consumer takes twice as long than the producer to process (100ms), then three data elements will be delayed (current process plus two 50ms data produced). That will account for up to 1/3 of the queue size, and can allow this up to three times. If the queue ends up being full, then this is mainly an architectual issue, not a queue sizing issue.


**Back-pressure policy**: queue full means... drop oldest? drop newest? block producer? log + drop? Justify.

-  If the queue is full, then the newest data being entered in the queue will simply be dropped. The producer is never blocked. This is done to avoid a backlog of old data and to preserve the newst data in the queue. The dropped data is logged as an incremented count.

**Event-group vs N semaphores**: explain why the event group is the better fit for the producer-consumer rendezvous.

-  It is better because it specifies a certain event that occurs and can allow a set bit signaling that is identifiable through FreeRTOS (so probably faster than implementing your own). If semaphores were used, you can signal how many tasks are involved but not what events or specific tasks are triggered.

**Direct notification vs binary semaphore**: measure wake latency on both paths (you have the App 3 helper). Numbers in your README.

-  The latency is about 15 ms end-to-end, with the Direct Notif path taking about 10 ms and the semaphore taking about 25 ms to execute per cycle.

# Engineering analysis prompts 
1. Why pin the web server to Core 0 vs Core 1? (The scaffold puts the monitor on Core 0 - defend or challenge that.)

-  Because it offloads work from one core to both; having one core do all the work risks overutilization and degredation of the system. It is also a good practive to dedicate cores to certain processes, such as low-level tasks vs high-level tasks and networks

2. Queue depth  & pressure - why did you select the size you did?

- I selected the 10 element queue size because with a 20Hz producer rate that equates to 500ms for a full queue. This means that theres is approximately 10x overhead for each data entering the queue, and any delays won't cause packet loss in the queue unless a severe process delay occurs. Anything more than 10 elements is an overkill as the whole entire (simulated) consumer process time takes from 40 - 100ms. 

3. Explain the pros/cons to Event group vs N semaphores.

- Event group allows for specification of actions/tasks during an event. Its bits can easily be identified and reconfigured for programming needs, such as blocking or signaling based on any n bits. However, it is complex and requires a detailed and purposeful need. On the otherhand, semaphores are useful if you only want to count the events/resources through a take and give structure. This is useful for things like button clicks or led flashing, but only one semaphore can be blocked for at any given time.

4. Direct notification vs binary semaphore — with measured numbers.

-  Direct notification is the fastest way to signal one task at < 15 microseconds, however it is limited as a many-to-one communication. Typically, this is reserved for dependent stages of a process, such as a deicated responder task with senders. Binary semaphore is a bit slower at < 50 ms, and is implemented using queues (so slower and some memory overhead). This is great for many-to-many communication with one resource. So if you wanted to flip an led or had a push button, a binary semaphore can indicate a push or toggle on as a resource taken or action occuring. 

# Block of codes origin and architecture

Everything from the scaffold is there, with the only changes added being the imported functions/task for the web server from App 1, the direct notif vs. semaphore path delay from app 3, and a proccess time variable for simulating the consumer's variable processing time and how it affects the queue.

The architecture follows a producer-consumer, coordinatorer-responder with variable process delay for induced failures. 