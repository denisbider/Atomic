# Atomic

Atomic is denis bider's love-child library of all the good stuff. It's written in C++ and targets Windows. It contains everything from foundational classes like `Seq`, `Vec`, `Enc`, `Str`, `Map`, to large features:

* A transactional database with three ascending levels of abstraction:

  * A raw `ObjectStore` that stores blobs by ID.
  
  * An intermediate `TreeStore` that provides hierarchy.
  
  * A full-fledged `EntityStore` that stores complex structures with fields in a hierarchy.

* An `HttpServer` using the Windows HTTP API.

* A `BhtpnServer` - binary HTTP over named pipes. It allows for service administration using built-in Windows security instead of an additional layer of authentication.

* A zero-copy, general-purpose parser that enables easy building of grammars in C++.

* Full grammars to parse the Internet Message Format (emails), including MIME and DSN (Delivery Status Notifications).

* Full grammars to parse HTML, URIs and my version of Markdown. Rudimentary grammars for CSS and JS.

* `ScriptPack`, a simple CSS and JS packer for C++.

* `SmtpSender`, `SmtpReceiver` and `Pop3Server` with TLS support using Schannel on Windows.

* Support for reading and generating email messages, including DKIM signing.

* `Originator`, a self-contained DLL which packs email message generation, SMTP sending functionality, and an outgoing mail queue.

* `Diff`, a general-purpose, sliding-matrix diff algorithm that produces good output and efficiently compares inputs of any size.

* [BusyBeaver](https://denisbider.blogspot.com/2015/05/busybeaver-key-derivation-function.html), my password hashing algorithm and key derivation function.

## Who is denis bider?

I am co-founder and President at Bitvise, where I have managed the development of Bitvise SSH Server and SSH Client since 2001. I have worked with C++ since the mid-1990s.

## Why make Atomic?

Atomic is a love child I created in my free time, spanning perhaps a decade up to 2019. During this time, I was its only developer and user. I published it first in November 2019 because Bitvise has begun to use significant aspects of it, so I'm getting an external dependency anyhow. And then, because it's beautiful and I want people to see it.

## Why not publish it earlier?

I like being free to develop Atomic as I see fit, sometimes to perform substantial refactoring if I see an opportunity. When I work for Bitvise, I serve customers and their wants and needs. When I work on this, I serve my ideas of beauty. I felt like I have enough customers in my work, and don't need more in my free time.

## What if I want support?

Atomic is maintained. It is going to continue to be. If you have a defect report, I welcome it.

*However*, I maintain it for my own needs, not to serve users. If you seek someone's help, or you want me to do something for you, then if you are on Windows, please direct such requests to a file named NUL.

## What compilers and platforms does it support?

I currently work on Atomic in Visual Studio 2015 on Windows. My target platforms are desktop and server versions of Windows x86 and x64, currently starting from Windows Vista, up to and including the latest.

The following is a list of things I'm *not* interested in:

- Other versions of Visual Studio. I'll care about newer ones when I upgrade. (And then I won't care about the old.)

- Other compilers and environments. I don't care about GCC, clang, LLVM, or MinGW. You're free to use that. Your problem.

- Target platforms other than Windows. I don't care about Linux, iOS, or Android. You're free to use that. Your problem.

- Target architectures other than Intel x86 and AMD x64. I don't care about ARM or Arduino or Raspberry or Vortex. You're free to use that. Your problem.

## Why did you make `Seq`, `Str`, `Vec`, `Map` instead of using STL equivalents?

I have my own ideas about stuff and I like to implement them.

`Seq` predates `std::string_view`, has a cooler name and implements more stuff. It's the reader/decoder/simple parser for everything byte- or character-oriented. Any function that receives a string should take a `Seq`.

`Str` is different from `std::string` in that it has more functionality, an interface more the way I like, and it inherits from `Enc`. The `Str` class is used for string ownership, not string reading or writing. Functions that take binary or character-oriented inputs should take a `Seq`; functions that produce binary or character-oriented outputs should take an `Enc&`. Sometimes it is acceptable for functions to return an `Str` when the result is likely to be owned separately rather than immediately appended to something.

`Enc` is a base class of `Str` which contains a subset of string functionality, specifically for writing, appending and encoding. It specifically does not provide an interface to clear the string or write from the beginning. Any function that writes binary or character-oriented output should take an `Enc&` to make it clear the contract is to append, not overwrite from the beginning.

`Vec` is like `std::vector`, but it specializes in moving and preventing copying. C++ was originally developed with a mis-design where copying objects was a first-class concept and moving wasn't even a thing. Probably 50% of efficient use of C++ is to avoid the language mis-feature where it automatically performs deep copies on objects. `Vec` is designed specifically to store objects that can be moved. Atomic treats move as a first-class concept that needs to be supported by everything.

`Map` is like `std::map`, but it requires the value type to contain the key, rather than storing it separately. Part of its purpose is, again, to treat moving as a first-class concept and prevent stuff from being accidentally copied. It performs similarly to `std::map` in Visual Studio.

## This code is for Windows, but its internal character representation is UTF-8?

Yes. I recently stumbled across the [UTF-8 Everywhere](http://utf8everywhere.org/) manifesto: I had nothing to do with it, but I could have wriiten it myself.

In a nutshell, the Windows decision to use UCS-2, and later UTF-16, was a well-meant historical mistake. It used to look like 65536 code points would be enough for everybody. *chuckle* If it was, UCS-2 would have worked great. But it is not, and then UTF-8 is best.

## Is this code production quality?

*Yes*, in the sense that 90% of what everyone uses in production is worse crap. This code is fundamentally of high quality, if perhaps with a bug here and there.

*No*, if you're going to use it on a mission to send humans to Mars. While I'm a perfectionist, I also wouldn't bet lives on it because I don't like seeing people die.

But stuff gets done by non-perfectionists. We routinely bet lives on code of [dubious quality](http://www.safetyresearch.net/blog/articles/toyota-unintended-acceleration-and-big-bowl-%E2%80%9Cspaghetti%E2%80%9D-code). Facebook was proverbially founded on PHP. People like me instead spend 10 years polishing an email parser like it's somehow not good enough.

## Does denis use this code in production?

Uhh... maybe. *gives a shifty look* Why do you ask? What do you know?

Atomic does not underpin Bitvise SSH Client and Server because it was developed separately and years later. However, parts of it are making way into Bitvise's software. This is part of why I'm publishing it - I'm getting external dependencies anyway.

## Is there more of Atomic that's not published?

Yes. Among other things, there's Integral. It's a high-performance C++ web server, email server, content management system, online forum, activation system and maybe even a case management system (if I finish that).

Integral may be published if or when I think it's time. If it's published, it might be as open source or commercial software or a combination. This might be influenced by how direly I want cash and users, and how much time I want to dedicate to that.
