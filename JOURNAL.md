---

title: "SAM32 Design Journal"

author: "samhithe"

description: "Designing a custom computer from scratch (CPU, OS, assembler, emulator, etc.)"

created_at: "2026-06-02"

---



# SAM32 Design Journal
building a whole computer from scratch is crazy man



## Total Design Hours: ~0 hours



## Entry 1

## 6/2/26 - Designing the Core ISA

First entry into the journal! Today I designed the core SAM32 instruction set architecture (ISA).

I decided to go with something I already knew. I have a lot of experience with RISC-V, and I had designed a CPU for an ARM-like ISA in the game *Turing Complete*, so it seemed logical to go with a hybrid of those. The ISA changed, a LOT, throughout the project. The ISA at this point was very different from my current (6/14/26) ISA. Note, all of the entries are devlogs, and say zero hours as a result.

time spent: 0 hours



## Entry 2

## 6/3/26 - Planning the Project

I had an ISA, but I didn't know what to do with it. My original plan was to build just a CPU, but I have a lot of time for the summer. I then got the idea to make my own computer! Not just plug a bunch of parts into a motherboard, but to design and build the entire thing from scratch! 

Obviously, I won't be designing a motherboard, RAM sticks, a GPU and so on, but I want to design and build my own computer. From there, it was a matter of thinking about what a computer needs to be useful:

* a CPU
* an assembler to assemble programs
* some form of an output
* an operating system to actually allow users to do something on it

It's missing one thing though -- How do I test my programs out? I can't stare at a CPU and magically figure out what value is stored in what register. I need a debugger, so the debugger naturally got added to the project. Subject to change. Once I make my OS, I want to extend the project even further, and maybe add more 'goals' after.

time spent: 0 hours



## Entry 3

## 6/4/26 - Starting the Assembler

I needed to build an assembler so that I don't have to write 1s and 0s all day. Before starting the project, I needed to figure out what tools I was going to use to make it. Most of the time, I wouldn't even have to answer this question, the answer would just be Rust, but I wanted to try something new this time, and stuck with C++. 

C++ wasn't entirely new to me, I knew the basics of C++ and CMake from many other projects, but I had never worked on a project of this scale with C++.

time spent: 0 hours



## Entry 4

## 6/8/26 - "Finishing" the Assembler

Great! The assembler is now done! I can move on to the Emulator/Debugger! or so I thought. I made a bunch of mistakes in the ISA that I had not verified ahead of time, and the assembler code in general was a whole bunch of spaghetti (except it didn't look good). 

I had also completely forgotten about assembler directives! Assembler directives were really important! They allow the programmer to tell the assembler where to write code, what parts of the memory to set aside, and so on. I tried fixing it, but it was clear a rewrite was necessary, so I scrapped all of the code I had and restarted.

time spent: 0 hours



## Entry 5

## 6/10/26 - Actually Finishing the Assembler

Okay, now the assembler was actually done. I had LST files, assembler directives, and MUCH better error-telling(?) and extensibility. I needed to verify to make sure my assembler worked by writing a 30-line file, transcribing it by hand, and then making sure the assembler had correctly written the machine code. 

I spent an hour debugging it wondering why it was writing in the wrong order before I realized I had transcribed into big-endian, and the CPU was little-endian. Once I had ironed everything out, it was time to move on to the emulator/debugger. 

I knew this was going to be a breeze, so I wanted to see if I could change something about it to keep it exciting, and I realized that it would be really cool if the entire thing also ran on the web. I had already heard of Emscripten and ImGui through other projects that I had either worked on, or seen online, so I was already a little familiar with how it worked. After wrestling with the build tools for an hour (it really wasn't that complicated tbh, I was just overcomplicating it), I had gotten CMake to build for both the web target and the native target. Nice.

>I'm honestly pretty comfortable with assembly, but wouldn't it be really cool to have a C-like language? Maybe I'll write a compiler for that! <br/>

time spent: 0 hours



## Entry 6

## 6/12/26 - Building out the Web Emulator

Most of this time was just grinding out the core logic for the emulator and hooking up ImGui docs. Adding time-travel debugging, the LST assembly viewer, and getting the UI to play nice with the web target took a significant chunk of time.

time spent: 0 hours



## Entry 7

## 6/14/26 - Finishing the Emulator/Debugger

I finished the emulator/debugger! To be honest, it doesn't have ALL of the features I was hoping for, but the unimplemented features are all features I can't implement until later (VGA output visualizer, C compiler, etc.). 

I built the project, wrote my github actions (messed it up thrice. You can go check the commit logs lol), and deployed the web-app to [https://sam32.samhithe.dev/](https://sam32.samhithe.dev/). It looks ugly, and it's just the default Emscripten template, but it works, so you can't complain. 

I'm writing this as I'm deciding my next steps. I've already got AMD Vivado set up on my computer (I use Arch, btw. that's not even a flex anymore the AUR is compromised and I may have some malware on my computer as I'm writing this), and I want to start writing the SystemVerilog. 

I prefer to test-in-production, so I want to see if I can get the FPGA board I've landed on (Nexys A7 AMD Artix 7, it's beautiful) to arrive on the same day I finish the CPU. The next week will probably be very depressing and uneventful; I made a list of components for the CPU and the order to do them in, and that's all I'll be doing besides my job.

>You might've noticed this entry doesn't say approximate, and that's because this is the first one I've written on the day of. All of the entries before this were written on June 14 2026, and are just what I remember thinking. The entries from now on should be more authentic. <br/>

time spent: 0 hours



## Entry 8

## 6/15/26 - SystemVerilog for the ALU, PC and Register File

I didn't have too much time to work on the actual project today, I had to work until 3, and had to go out with a couple friends today (daily pickleball session) after, so I didn't have too long. When I had some free time, I looked up all of the stuff I need to know. 

Firstly, I found out that I need to set up a 'DMAC' for reading from an SD card. This is kind of like a mini, super-dumb unit that just reads from the storage and writes that data to the memory, and vice versa (the CPU is wayy too fast and would waste a bunch of CPU cycles just updating stuff, so you instead dedicate this unit to do it instead, so that the CPU can instead focus on important stuff. Then, you tug an interrupt wire and then the CPU goes back to do whatever it needed to do with the storage). I've never been good with async concepts, so this will be a bit of a challenge for me. 

Besides doing this, I had an hour/hour+30min to work on the actual SystemVerilog, so I wrote the ALU, PC, and the RegisterFile. It took a lot less time than needed, mostly because I could reuse code from previous projects or just because I did the easy stuff first. All of the testbenches passed, and so I ate a brookie.

I DID use AI to generate edge cases that I may have missed. For example, I didn't think to make sure the PC loads with a zero (I can't think of a scenario where it wouldn't TBH). The 2 hours I logged for this is also not 100% spent on writing the actual code. a good 30-45 min was spent in my free time while working just reading old documentation or prompting to see exactly how I would do stuff (e.g., I found out about the DMAC, even though I used it a bunch without realizing it).

time spent: 0 hours

## Entry 9

## 6/16/26 - SystemVerilog for the CU

I didn't have very much time today either, as I had work. I finished the CU logic and testbench today (the testbench took forever!). All of the 'big bad' components are done, now all I have is a bunch of smaller, simpler components.

time spent: 0 hours

## Entry 10

## 6/19/26 - RAM module

In case you're wondering what's been happening the past couple days, I've finished the ALU, CU, Pc register, ROM (program memory), and the register file, but I've been struggling with RAM. I've experimented heavily with MIG (Memory Interface Generator, not Mikoyan-Gurevich) and how I can make modules with it, and unfortunately, most of my test modules were just not it. Eventually, I settled with the premade SRAM-to-DDR module given by Digilent, as that simplifies everything by a lot. I had really hoped for my RAM to be fully under my control and designed by me, but unfortunately, it resulted me being behind my schedule for what modules I wanted to make on what days, so I had to settle for pre-made modules. On the bright side, I changed some stuff in the vhd file to fit my ISA a little better. I did need to use ChatGPT's help here, as I did not know VHDL, and I felt like I was looking at heiroglyphs when I opened the file.

time spent: 0 hours