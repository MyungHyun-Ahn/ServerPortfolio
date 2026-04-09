import { randomUUID } from "crypto";

import { appEnv } from "../config/env";
import { getRedisClient } from "../db/redis";
import type { AuthenticatedAccount, ChatServerEndpoint } from "../models/auth-types";

export interface IssuedChatTicket {
  ticket: string;
  ttlSeconds: number;
  chatServer: ChatServerEndpoint;
}

const kActiveLoginKeyPrefix = "chat:active-login:";

export class ChatTicketService {
  public async issueTicket(account: AuthenticatedAccount): Promise<IssuedChatTicket> {
    const ticket = randomUUID();
    const redisClient = await getRedisClient();
    const loginVersion = await redisClient.incr(this.buildActiveLoginKey(account.userId));

    await redisClient.set(this.buildTicketKey(ticket), this.buildTicketPayload(account.userId, loginVersion), {
      EX: appEnv.ticket.ttlSeconds,
    });

    return {
      ticket,
      ttlSeconds: appEnv.ticket.ttlSeconds,
      chatServer: {
        ip: appEnv.chatServer.ip,
        port: appEnv.chatServer.port,
      },
    };
  }

  private buildTicketKey(ticket: string): string {
    return `${appEnv.ticket.keyPrefix}${ticket}`;
  }

  private buildActiveLoginKey(userId: number): string {
    return `${kActiveLoginKeyPrefix}${userId}`;
  }

  private buildTicketPayload(userId: number, loginVersion: number): string {
    return `${userId}:${loginVersion}`;
  }
}
