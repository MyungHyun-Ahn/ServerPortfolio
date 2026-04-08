import { randomUUID } from "crypto";

import { appEnv } from "../config/env";
import { getRedisClient } from "../db/redis";
import type { AuthenticatedAccount, ChatServerEndpoint } from "../models/auth-types";

export interface IssuedChatTicket {
  ticket: string;
  ttlSeconds: number;
  chatServer: ChatServerEndpoint;
}

export class ChatTicketService {
  public async issueTicket(account: AuthenticatedAccount): Promise<IssuedChatTicket> {
    const ticket = randomUUID();
    const redisClient = await getRedisClient();

    // 현재 ChattingServer Redis consume 구현은 value 전체를 userId 문자열로 해석한다.
    await redisClient.set(this.buildTicketKey(ticket), account.userId.toString(), {
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
}
