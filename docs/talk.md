Of course. Here is a text-only storyboard for your presentation, structured slide by slide.

***

## Slide 1: Title Slide

* **Title:** RPC Revisited
* **Subtitle:** Getting things to talk to each other without in your face complexity.
* **Presenter:** Edward Boggis-Rolfe

---

## Slide 2: The Agenda

* A Brief History of RPC
* The Fall from Grace: Why Did It Go Out of Favor?
* A New Approach: Addressing the Core Problems
* Key Features & Innovations
* Use Case: Powering Secretarium's Secure Infrastructure
* How It Works: A Look at the Plumbing
* The Road Ahead: Roadmap & Challenges

---

## Slide 3: A Quick Background on RPC

* **What is it?** Remote Procedure Call. Thought up back in the 50's.
* **The Goal:** Make a function call on a remote server look exactly like a local function call in your code.
* **The Promise:** To completely hide the complexity of accessing functionallity that is not necessarily in the same address space.
* ****

---

## Slide 4: Why Did RPC Go Out of Favor? 🤔

* **Tight Coupling:** Implementations were often tied to a specific language, platform, or data format. (e.g., Java RMI).
* **Brittleness:** Changing a function signature on the server could easily break all clients. Versioning was a nightmare.
* **Synchronous Nature:** Most early RPCs were **blocking**. The application would freeze while waiting for the network, killing performance.
* **The "Leaky Abstraction":** By hiding the network, it encouraged developers to ignore network realities like latency and failure, leading to fragile applications.
* **The Rise of REST:** HTTP/JSON provided a more flexible, stateless, and human-readable alternative that conquered the web.

---

## Slide 5: The Solution: An RPC for the Modern Era

* I believe we've built a solution that captures the original simplicity of RPC while directly addressing all its historical failings.
* **Our Goal:** Provide a high-performance, flexible, and secure communication layer that works seamlessly across languages, transports, and platforms.
* It's not about hiding the network, but **taming its complexity**.

---

## Slide 6: Feature Deep Dive: Total Independence

* **Serialization Agnostic:** You are not locked into one format. Use Protobuf for performance, JSON for web compatibility, or any other format. The framework doesn't care.
* **Transport Agnostic:** Run over high-speed TCP, WebSockets for real-time web clients, or even message queues. Your application code remains identical.
* **Language Agnostic:** Built from the ground up to encourage support for other languages. Write your service in C# and call it from Python, Rust, or TypeScript.

---

## Slide 7: Feature Deep Dive: A Modern Architecture

* **Async First:** All operations are asynchronous by default to ensure high performance and scalability.
* **Multi-hop Calls:** A single client call can be securely and efficiently routed through a chain of internal services before reaching its destination—essential for microservice architectures.
* **Rich Object Model:**
    * **Polymorphic:** Pass interfaces and derived types over the wire.
    * **Multiple Interfaces:** A single object can expose multiple, distinct APIs.
* **Built-in Reflection:** Services can be discovered and queried at runtime, enabling powerful tooling and dynamic clients.

---

## Slide 8: Feature Deep Dive: Enterprise Grade

* **Secure:** Designed with security as a primary concern, not an afterthought.
* **Excellent Telemetry:** Rich, built-in logging and metrics to understand exactly what's happening in your system.
* **Backwardly Compatible:** Provides strategies for evolving your APIs without breaking existing clients.
* **Open Source:** Transparent, community-driven, and free from vendor lock-in.
* **Web & MCP Compatible:** Works flawlessly in browsers and specialized Mainframe/Midrange environments (MCP).

---

## Slide 9: Use Case: Why Secretarium Needed This

* **Who we are:** Secretarium is a deep-tech company providing a platform for confidential computing, allowing multiple parties to process sensitive data without ever exposing it.
* **The Challenge:** We needed a communication protocol for our distributed network of secure enclaves that was extremely fast, highly secure, and flexible enough to handle complex, stateful interactions.
* **Why others failed:**
    * REST/JSON was too slow and verbose.
    * gRPC was too rigid and didn't support our dynamic object model or multi-hop requirements.
* **The Solution:** We built this RPC framework internally to be the backbone of our entire platform.

---

## Slide 10: How the Plumbing Works 🔧

* A simplified, high-level overview of a call:
    1.  **The Client:** Calls a method on a local proxy object.
    2.  **Serialization:** The framework serializes the method name and its arguments into your chosen format (e.g., Protobuf).
    3.  **The Envelope:** The serialized data is wrapped in an "envelope" containing routing, security, and context metadata.
    4.  **Transport:** The envelope is sent over the wire using the configured transport (e.g., a TCP socket).
    5.  **The Server:** Receives the envelope, deserializes it, finds the target object via its unique ID, and invokes the correct method with the arguments.
    6.  The response travels back the same way.

---

## Slide 11: Managing Interfaces: Shared vs. Optimistic

* Our framework supports two powerful modes for interface management:
* **Shared Interfaces (Strict):**
    * Both client and server reference a common, shared library or contract.
    * **Pro:** Guarantees compile-time safety and compatibility.
    * **Con:** Requires a coordinated update of both client and server.
* **Optimistic Interfaces (Flexible):**
    * The client works with a cached or "best-guess" version of the server's interface.
    * **Pro:** Allows services to evolve independently. Perfect for highly dynamic microservice environments.
    * **Con:** Errors are discovered at runtime if a major breaking change occurs.
* **Namespacing** is used throughout to keep everything organized and avoid collisions.

---

## Slide 12: Roadmap and Challenges 🗺️

* **Immediate Goals:**
    * Expand language support with official SDKs for Python and Rust.
    * Enhance documentation with more tutorials and real-world examples.
* **Future Vision:**
    * Integration with service meshes like Istio and Linkerd.
    * Tooling for automatic client generation and API portals.
* **Challenges:**
    * Building and fostering a vibrant open-source community.
    * Educating developers on the benefits of this new model over established players like gRPC and REST.

---

## Slide 13: Summary & Call to Action

* Traditional RPC had the right idea but the wrong implementation for its time.
* We've revisited the concept, solving the core problems of **coupling, brittleness, and blocking I/O**.
* The result is a flexible, secure, and high-performance framework fit for modern distributed systems.
* **Get Involved!**
    * Check out the project on GitHub: [Link to Repo]
    * Read the docs: [Link to Docs]
    * Join the conversation.

---

## Slide 14: Thank You

* **Questions?**